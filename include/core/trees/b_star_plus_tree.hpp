#pragma once
#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>
#include <associative_container.h>
#include <storage/page.hpp>
#include <storage/page_io.hpp>
#include <storage/pager.hpp>
#include <storage/serializer.hpp>
#include <optional>
#include <filesystem>
#include <stdexcept>
#include <tuple>
#include <type_traits>

template<typename TKey, typename TValue, comparator<TKey> compare = std::less<TKey>, std::size_t t = 5>
class BSP_tree final : compare {
public:
    using tree_data_type = std::pair<TKey, TValue>;
    using tree_data_type_const = std::pair<const TKey, TValue>;
    using value_type = tree_data_type_const;

private:
    // region disk structs declaration

    enum class bsptree_page_type : std::uint8_t {
        term,
        middle,
        free
    };

    struct bsptree_file_header {
        std::uint64_t _magic{};
        std::uint32_t _version{};
        std::uint64_t _tree_order{};
        std::uint64_t _page_layout{};
        std::uint64_t _term_capacity{};
        std::uint64_t _middle_capacity{};
        storage::page_id_t _root_page{storage::invalid_page_id};
        storage::page_id_t _first_term{storage::invalid_page_id};
        storage::page_id_t _next_page{storage::first_data_page_id};
        storage::page_id_t _first_free_page{storage::invalid_page_id};
        std::uint64_t _size{};
    };

    struct bsptree_node_header {
        bsptree_page_type _type{bsptree_page_type::term};
        std::uint16_t _keys_count{};
        storage::page_id_t _self{storage::invalid_page_id};
        storage::page_id_t _parent{storage::invalid_page_id};
    };

    struct bsptree_node_term {
        bsptree_node_header _header{};
        storage::page_id_t _next{storage::invalid_page_id};
        std::vector<tree_data_type> data;
    };

    struct bsptree_node_middle {
        bsptree_node_header _header{};
        std::vector<TKey> _keys;
        std::vector<storage::page_id_t> _pointers;
    };

    // endregion disk structs declaration

    // region constants declaration

    static constexpr size_t minimum_keys_in_node = 2 * t - 1;
    static constexpr size_t maximum_keys_in_node = 3 * t - 1;

    static constexpr std::size_t minimum_pointers_in_node = minimum_keys_in_node + 1;
    static constexpr std::size_t maximum_pointers_in_node = maximum_keys_in_node + 1;

    static constexpr std::uint64_t disk_magic = 0x4253505452454531ULL; // BSPTREE1
    static constexpr std::uint32_t disk_version = 3;
    static constexpr std::uint64_t disk_variable_page_layout = 1;
    static constexpr std::size_t disk_term_header_size =
            sizeof(bsptree_page_type) + sizeof(std::uint16_t) + 3 * sizeof(storage::page_id_t);
    // endregion constants declaration

    // region fields declaration

    bool compare_keys(const TKey &lhs, const TKey &rhs) const { return compare::operator()(lhs, rhs); }

    storage::Pager _pager;
    bsptree_file_header _disk_header{};
    bool _copy_on_write_enabled{true};
    // endregion fields declaration
public:
    // region constructors declaration

    explicit BSP_tree(const std::filesystem::path &path, const compare &cmp = compare());

    BSP_tree(const std::filesystem::path &path, const BSP_tree &other, const compare &cmp = compare());

    // endregion constructors declaration

    // region five declaration

    BSP_tree(const BSP_tree &other) = delete;

    BSP_tree(BSP_tree &&other) noexcept;

    BSP_tree &operator=(const BSP_tree &other) = delete;

    BSP_tree &operator=(BSP_tree &&other) noexcept;

    ~BSP_tree() noexcept = default;

    void validate() const {
        validate_header();
        validate_tree();
    }

    // endregion five declaration

    // region iterators declaration

    class bsptree_iterator final {
        size_t _index{};
        const BSP_tree *_owner{};
        storage::page_id_t _page_id{storage::invalid_page_id};
        storage::page_id_t _next_page_id{storage::invalid_page_id};
        std::vector<tree_data_type> _disk_data;

    public:
        using value_type = tree_data_type;
        using reference = const value_type &;
        using pointer = const value_type *;
        using iterator_category = std::forward_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = bsptree_iterator;

        friend class BSP_tree;

        reference operator*() const;

        pointer operator->() const;

        self &operator++();

        self operator++(int);

        bool operator==(const self &other) const noexcept;

        explicit bsptree_iterator() noexcept = default;

    private:
        void load_disk_term(storage::page_id_t page_id);

        explicit bsptree_iterator(const BSP_tree *owner, storage::page_id_t page_id, size_t index,
                                  bsptree_node_term &&term);
    };

    // endregion iterators declaration

    // region element access declaration

    TValue at(const TKey &key) const {
        const auto it = find(key);
        if (it == end()) throw std::out_of_range("key not found");
        return it->second;
    }

    // endregion element access declaration

    // region iterator begins declaration

    bsptree_iterator begin() const {
        static_assert(std::is_copy_constructible_v<TKey> && std::is_copy_constructible_v<TValue>,
                      "disk-backed BSP_tree iterators require copy-constructible key and value types");
        return _disk_header._size == 0 || _disk_header._first_term == storage::invalid_page_id
                   ? end()
                   : bsptree_iterator(this, _disk_header._first_term, 0, read_term(_disk_header._first_term));
    }

    bsptree_iterator end() const { return bsptree_iterator(); }
    // endregion iterator begins declaration

    // region lookup declaration

    [[nodiscard]] size_t size() const noexcept { return _disk_header._size; }

    [[nodiscard]] bool empty() const noexcept { return _disk_header._size == 0; }

    bsptree_iterator find(const TKey &key) const {
        auto it = bound(key);
        return it != end() && keys_equal(key, it->first) ? it : end();
    }

    bsptree_iterator lower_bound(const TKey &key) const { return bound(key, false); }

    bsptree_iterator upper_bound(const TKey &key) const { return bound(key, true); }

    bool contains(const TKey &key) const { return find(key) != end(); }
    // endregion lookup declaration

    // region modifiers declaration

    void clear();

    std::pair<bsptree_iterator, bool> insert(tree_data_type data);

    template<typename... Args>
    std::pair<bsptree_iterator, bool> emplace(Args &&... args) {
        return insert(tree_data_type(std::forward<Args>(args)...));
    }

    bsptree_iterator insert_or_assign(tree_data_type data);

    template<typename... Args>
    bsptree_iterator emplace_or_assign(Args &&... args) {
        return insert_or_assign(tree_data_type(std::forward<Args>(args)...));
    }

    bsptree_iterator erase(bsptree_iterator pos) { return pos == end() ? end() : erase(pos->first); }

    bsptree_iterator erase(bsptree_iterator beg, bsptree_iterator en);

    bsptree_iterator erase(const TKey &key);

    // endregion modifiers declaration
private:
    // region helpers declaration

    BSP_tree(const std::filesystem::path &path, const compare &cmp, bool is_new_file, bool copy_on_write = true);

    template<typename Fn>
    void run_copy_on_write(Fn &&fn);

    bsptree_iterator bound(const TKey &key, bool strict = false) const;

    bool keys_equal(const TKey &a, const TKey &b) const noexcept { return !compare_keys(a, b) && !compare_keys(b, a); }

    static bsptree_node_term make_term(const storage::page_id_t self, const storage::page_id_t parent,
                                            const storage::page_id_t next = storage::invalid_page_id) {
        return {._header = {bsptree_page_type::term, 0, self, parent}, ._next = next};
    }

    static bsptree_node_middle make_middle(const storage::page_id_t self, const storage::page_id_t parent) {
        return {._header = {bsptree_page_type::middle, 0, self, parent}};
    }

    std::tuple<storage::page_id_t, bsptree_node_term, size_t> get_disk_term_and_index(const TKey &key) const;

    static void validate_disk_payload(const tree_data_type &data);

    void distribute_term_pair(bsptree_node_term &left, bsptree_node_term &right) const;

    void distribute_term_triple_pair(bsptree_node_term &left, bsptree_node_term &middle,
                                             bsptree_node_term &right) const;

    void split_term_pair_triple(bsptree_node_term &left, bsptree_node_term &middle,
                                        bsptree_node_term &right) const;

    void distribute_middle_pair(bsptree_node_middle &left, bsptree_node_middle &right);

    void distribute_middle_triple_pair(bsptree_node_middle &left, bsptree_node_middle &middle,
                                               bsptree_node_middle &right);

    void split_middle_pair_triple(bsptree_node_middle &left, bsptree_node_middle &middle,
                                          bsptree_node_middle &right);

    void set_children_parent(const bsptree_node_middle &node);

    // endregion helpers declaration

    // region disk helpers declaration

    static bool is_new_disk_file(const std::filesystem::path &path) {
        return !std::filesystem::exists(path) || std::filesystem::file_size(path) == 0;
    }

    void load_header();

    void write_header();

    void validate_header() const;

    void validate_tree() const;

    std::uint64_t validate_subtree(storage::page_id_t page_id, storage::page_id_t expected_parent, bool is_root,
                                        std::vector<bool> &reachable_pages) const;

    storage::page_id_t allocate_page();

    void deallocate_page(storage::page_id_t page_id);

    void release_disk_subtree(storage::page_id_t page_id);

    bsptree_page_type read_page_type(storage::page_id_t page_id) const;

    bsptree_node_term read_term(storage::page_id_t page_id) const;

    void write_term(storage::page_id_t page_id, const bsptree_node_term &node);

    bsptree_node_middle read_middle(storage::page_id_t page_id) const;

    void write_middle(storage::page_id_t page_id, const bsptree_node_middle &node);

    TKey subtree_first_key(storage::page_id_t page_id) const;

    void set_node_parent(storage::page_id_t page_id, storage::page_id_t parent_page);

    void rebuild_separator_keys(storage::page_id_t page_id);

    void rebuild_middle_keys(bsptree_node_middle &node);

    void wrebuilt_middle(bsptree_node_middle &node) {
        rebuild_middle_keys(node);
        write_middle(node._header._self, node);
    }

    void rebalance_term(storage::page_id_t term_page, bsptree_node_term &&term);

    void rebalance_middle(storage::page_id_t middle_page, bsptree_node_middle &&node);

    void split_root_term(bsptree_node_term &&term);

    void split_term_node(storage::page_id_t term_page, bsptree_node_term &&term);

    void split_root_middle(bsptree_node_middle &&root);

    void split_middle_node(storage::page_id_t middle_page, bsptree_node_middle &&node);

    // endregion disk helpers declaration
};

// region BSP_tree constructor implementations
template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSP_tree(const std::filesystem::path &path, const compare &cmp) : BSP_tree(
    path, cmp, is_new_disk_file(path), true) {
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSP_tree(const std::filesystem::path &path, const compare &cmp,
                                             const bool is_new_file, const bool copy_on_write) : compare(cmp),
    _pager(path),
    _copy_on_write_enabled(copy_on_write) {
    if (!is_new_file) {
        load_header();
        validate();
        return;
    }

    if (const auto header_page = _pager.allocate_page(); header_page != storage::header_page_id) {
        throw std::runtime_error("Unexpected header page id");
    }

    const auto root_page = allocate_page();
    _disk_header = {
        ._magic = disk_magic,
        ._version = disk_version,
        ._tree_order = t,
        ._page_layout = disk_variable_page_layout,
        ._term_capacity = maximum_keys_in_node,
        ._middle_capacity = maximum_keys_in_node,
        ._root_page = root_page,
        ._first_term = root_page,
        ._next_page = root_page + 1,
        ._first_free_page = storage::invalid_page_id,
        ._size = 0
    };

    write_term(root_page, make_term(root_page, storage::invalid_page_id));
    write_header();
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSP_tree(const std::filesystem::path &path, const BSP_tree &other,
                                             const compare &cmp) : BSP_tree(path, cmp, is_new_disk_file(path)) {
    if (std::filesystem::exists(path) && std::filesystem::equivalent(path, other._pager.path())) {
        throw std::invalid_argument("BSP_tree destination path must differ from source path");
    }

    clear();

    for (auto it = other.begin(); it != other.end(); ++it) {
        insert({it->first, it->second});
    }
}

// endregion BSP_tree constructor implementations

// region BSP_tree copy and move constructors
template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare,
    t>::BSP_tree(BSP_tree &&other) noexcept : compare(std::move(static_cast<compare &>(other))),
                                              _pager(std::move(other._pager)),
                                              _disk_header(std::exchange(other._disk_header, bsptree_file_header{})),
                                              _copy_on_write_enabled(
                                                  std::exchange(other._copy_on_write_enabled, true)) {
}

// endregion BSP_tree copy and move constructors

// region BSP_tree copy and move assignment operators
template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t> &BSP_tree<TKey, TValue, compare, t>::operator=(BSP_tree &&other) noexcept {
    if (this == &other) return *this;
    BSP_tree tmp(std::move(other));
    std::swap(static_cast<compare &>(*this), static_cast<compare &>(tmp));
    std::swap(_pager, tmp._pager);
    std::swap(_disk_header, tmp._disk_header);
    std::swap(_copy_on_write_enabled, tmp._copy_on_write_enabled);
    return *this;
}

// endregion BSP_tree copy and move assignment operators

// region BSP_tree five helpers implementations
template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
template<typename Fn>
void BSP_tree<TKey, TValue, compare, t>::run_copy_on_write(Fn &&fn) {
    const auto original_path = _pager.path();
    auto temp_path = original_path;
    temp_path += ".cow.tmp";

    std::filesystem::copy_file(original_path, temp_path, std::filesystem::copy_options::overwrite_existing);
    try {
        {
            BSP_tree shadow(temp_path, static_cast<const compare &>(*this), false, false);
            std::forward<Fn>(fn)(shadow);
            shadow.validate();
        }
        std::filesystem::rename(temp_path, original_path);
        _pager = storage::Pager(original_path);
        load_header();
        validate();
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temp_path, ignored);
        throw;
    }
}

// endregion BSP_tree five helpers implementations

// region BSP_tree iterators implementations
template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::bsptree_iterator::bsptree_iterator(const BSP_tree *owner,
                                                                       const storage::page_id_t page_id,
                                                                       const size_t index,
                                                                       bsptree_node_term &&term) : _index(index),
    _owner(owner), _page_id(page_id), _next_page_id(term._next), _disk_data(std::move(term.data)) {
    if (!owner || page_id == storage::invalid_page_id || _disk_data.empty()) {
        _page_id = storage::invalid_page_id;
        _next_page_id = storage::invalid_page_id;
        _index = 0;
    }
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::bsptree_iterator::load_disk_term(const storage::page_id_t page_id) {
    static_assert(std::is_copy_constructible_v<TKey> && std::is_copy_constructible_v<TValue>,
                  "disk iterator requires copy-constructible key and value types");
    const auto term = _owner->read_term(page_id);
    _page_id = page_id;
    _next_page_id = term._next;
    _disk_data = term.data;
    if (_disk_data.empty()) {
        _page_id = storage::invalid_page_id;
        _next_page_id = storage::invalid_page_id;
        _index = 0;
    }
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::bsptree_iterator::reference BSP_tree<TKey, TValue, compare,
    t>::bsptree_iterator::operator*() const {
    return _disk_data[_index];
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::bsptree_iterator::pointer BSP_tree<TKey, TValue, compare,
    t>::bsptree_iterator::operator->() const {
    return &_disk_data[_index];
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::bsptree_iterator &BSP_tree<TKey, TValue, compare, t>::bsptree_iterator::operator
++() {
    if (_page_id == storage::invalid_page_id) return *this;

    if (_index + 1 < _disk_data.size()) {
        ++_index;
        return *this;
    }
    if (_next_page_id == storage::invalid_page_id) {
        _page_id = storage::invalid_page_id;
        _index = 0;
        _disk_data.clear();
        return *this;
    }
    load_disk_term(_next_page_id);
    _index = 0;
    return *this;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::bsptree_iterator BSP_tree<TKey, TValue, compare, t>::
bsptree_iterator::operator++(int) {
    self tmp = *this;
    ++*this;
    return tmp;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
bool BSP_tree<TKey, TValue, compare, t>::bsptree_iterator::operator==(const self &other) const noexcept {
    if (_page_id == storage::invalid_page_id && other._page_id == storage::invalid_page_id) return true;
    return _owner == other._owner && _page_id == other._page_id && _index == other._index;
}

// endregion BSP_tree iterators implementations

// region BSP_tree modifiers implementations
template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::clear() {
    if (_copy_on_write_enabled) {
        run_copy_on_write([](BSP_tree &shadow) { shadow.clear(); });
        return;
    }

    if (_disk_header._root_page != storage::invalid_page_id) {
        release_disk_subtree(_disk_header._root_page);
    }

    const auto root_page = allocate_page();
    _disk_header._root_page = root_page;
    _disk_header._first_term = root_page;
    _disk_header._size = 0;

    write_term(root_page, make_term(root_page, storage::invalid_page_id));
    write_header();
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
std::pair<typename BSP_tree<TKey, TValue, compare, t>::bsptree_iterator, bool> BSP_tree<TKey, TValue, compare,
    t>::insert(tree_data_type data) {
    static_assert(std::is_copy_constructible_v<TKey> && std::is_copy_constructible_v<TValue>,
                  "disk-backed BSP_tree insertion requires copy-constructible key and value types");

    const auto k_copy = data.first;
    validate_disk_payload(data);
    if (_copy_on_write_enabled) {
        if (auto it = find(k_copy); it != end()) {
            return {it, false};
        }

        run_copy_on_write([payload = std::move(data)](BSP_tree &shadow) mutable {
            shadow.insert(std::move(payload));
        });

        return {find(k_copy), true};
    }

    const auto existing = find(k_copy);
    if (existing != end()) return {existing, false};

    auto [term_page, term, idx] = get_disk_term_and_index(k_copy);

    term.data.insert(term.data.begin() + idx, std::move(data));
    ++_disk_header._size;

    if (term.data.size() <= maximum_keys_in_node) {
        write_term(term_page, term);
    } else if (term_page == _disk_header._root_page) {
        split_root_term(std::move(term));
    } else {
        split_term_node(term_page, std::move(term));
    }

    write_header();

    return {find(k_copy), true};
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::bsptree_iterator BSP_tree<TKey, TValue, compare, t>::insert_or_assign(
    tree_data_type data) {
    static_assert(std::is_move_assignable_v<TValue>,
                  "disk-backed BSP_tree insert_or_assign requires move-assignable value type");

    validate_disk_payload(data);
    const auto k_copy = data.first;
    if (_copy_on_write_enabled) {
        run_copy_on_write([payload = std::move(data)](BSP_tree &shadow) mutable {
            shadow.insert_or_assign(std::move(payload));
        });
        return find(k_copy);
    }

    auto [term_page, term, index] = get_disk_term_and_index(data.first);

    if (index >= term.data.size() || !keys_equal(term.data[index].first, data.first)) {
        return insert(std::move(data)).first;
    }

    term.data[index].second = std::move(data.second);
    write_term(term_page, term);
    return find(k_copy);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::bsptree_iterator BSP_tree<TKey, TValue, compare, t>::erase(
    bsptree_iterator beg, bsptree_iterator en) {
    if (beg == en) return beg;

    std::optional<TKey> end_value;
    if (en != this->end()) end_value = en->first;

    while (beg != this->end()) {
        if (end_value && !compare_keys(beg->first, end_value)) break;
        beg = erase(beg);
    }

    return end_value ? lower_bound(end_value) : this->end();
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::bsptree_iterator BSP_tree<TKey, TValue, compare, t>::erase(const TKey &key) {
    static_assert(std::is_copy_constructible_v<TKey> && std::is_copy_constructible_v<TValue>,
                  "disk-backed BSP_tree erase requires copy-constructible key and value types");

    if (_disk_header._size == 0) return end();

    const auto it = find(key);
    if (it == end()) return end();

    std::optional<TKey> next_key;
    const auto ub = upper_bound(key);
    if (ub != end()) next_key = ub->first;

    if (_copy_on_write_enabled) {
        run_copy_on_write([&key](BSP_tree &shadow) {
            shadow.erase(key);
        });
        if (!next_key) return end();
        return lower_bound(*next_key);
    }

    auto [term_page, term, index] = get_disk_term_and_index(key);

    if (index >= term.data.size() || !keys_equal(term.data[index].first, key)) return end();

    term.data.erase(term.data.begin() + index);
    --_disk_header._size;
    rebalance_term(term_page, std::move(term));
    if (read_page_type(_disk_header._root_page) == bsptree_page_type::middle) {
        auto root = read_middle(_disk_header._root_page);
        while (root._pointers.size() == 1) {
            const auto old_root_page = _disk_header._root_page;
            _disk_header._root_page = root._pointers.front();
            set_node_parent(_disk_header._root_page, storage::invalid_page_id);
            deallocate_page(old_root_page);
            if (read_page_type(_disk_header._root_page) == bsptree_page_type::term) {
                _disk_header._first_term = _disk_header._root_page;
                break;
            }
            root = read_middle(_disk_header._root_page);
        }
    }
    rebuild_separator_keys(_disk_header._root_page);
    write_header();

    if (!next_key) return end();
    return lower_bound(*next_key);
}

// endregion BSP_tree modifiers implementations

// region BSP_tree helpers implementations
template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::bsptree_iterator
BSP_tree<TKey, TValue, compare, t>::bound(const TKey &key, const bool strict) const {
    if (_disk_header._size == 0) return end();

    auto [term_page, term, index] = get_disk_term_and_index(key);
    if (strict) {
        index = std::ranges::upper_bound(term.data, key,
                                         [this](const TKey &a, const TKey &b) { return compare_keys(a, b); },
                                         [](const tree_data_type &p) { return p.first; }) - term.data.begin();
    }

    if (index < term.data.size()) return bsptree_iterator(this, term_page, index, std::move(term));
    if (term._next != storage::invalid_page_id) {
        const auto next_page = term._next;
        return bsptree_iterator(this, next_page, 0, read_term(next_page));
    }
    return end();
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
std::tuple<storage::page_id_t, typename BSP_tree<TKey, TValue, compare, t>::bsptree_node_term, size_t>
BSP_tree<TKey, TValue, compare, t>::get_disk_term_and_index(const TKey &key) const {
    auto term_page = _disk_header._root_page;
    while (read_page_type(term_page) == bsptree_page_type::middle) {
        const auto middle = read_middle(term_page);
        const auto pointer_it = std::ranges::upper_bound(middle._keys, key, [this](const TKey &a, const TKey &b) {
            return compare_keys(a, b);
        });
        term_page = middle._pointers[pointer_it - middle._keys.begin()];
    }

    auto term = read_term(term_page);
    const auto index = std::ranges::lower_bound(
                           term.data,
                           key,
                           [this](const TKey &a, const TKey &b) { return compare_keys(a, b); },
                           [](const tree_data_type &p) { return p.first; }) -
                       term.data.begin();
    return {term_page, std::move(term), index};
}

// endregion BSP_tree helpers implementations

// region disk serialization helpers implementations
template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::validate_disk_payload(const tree_data_type &data) {
    const auto key = storage::Serializer<TKey>::to_bytes(data.first);
    if (const auto value = storage::Serializer<TValue>::to_bytes(data.second);
        disk_term_header_size + 2 * sizeof(std::uint32_t) + key.size() + value.size() > storage::page_size) {
        throw std::runtime_error("Serialized BSP_tree key/value pair is too large for one disk page");
    }
}

// endregion disk serialization helpers implementations

// region distribution helpers implementations
template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::distribute_term_pair(bsptree_node_term &left,
                                                                   bsptree_node_term &right) const {
    std::vector<tree_data_type> combined;
    combined.reserve(left.data.size() + right.data.size());
    for (auto &item: left.data) combined.push_back(std::move(item));
    for (auto &item: right.data) combined.push_back(std::move(item));

    const auto left_size = combined.size() / 2;
    left.data.assign(std::make_move_iterator(combined.begin()), std::make_move_iterator(combined.begin() + left_size));
    right.data.assign(std::make_move_iterator(combined.begin() + left_size), std::make_move_iterator(combined.end()));
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::distribute_term_triple_pair(
    bsptree_node_term &left,
    bsptree_node_term &middle,
    bsptree_node_term &right) const {
    std::vector<tree_data_type> combined;
    combined.reserve(left.data.size() + middle.data.size() + right.data.size());
    for (auto &item: left.data) combined.push_back(std::move(item));
    for (auto &item: middle.data) combined.push_back(std::move(item));
    for (auto &item: right.data) combined.push_back(std::move(item));

    const auto left_size = combined.size() / 2;
    left.data.assign(std::make_move_iterator(combined.begin()), std::make_move_iterator(combined.begin() + left_size));
    middle.data.assign(std::make_move_iterator(combined.begin() + left_size), std::make_move_iterator(combined.end()));
    left._next = middle._header._self;
    middle._next = right._next;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::split_term_pair_triple(
    bsptree_node_term &left,
    bsptree_node_term &middle,
    bsptree_node_term &right) const {
    std::vector<tree_data_type> combined;
    combined.reserve(left.data.size() + right.data.size());
    for (auto &item: left.data) combined.push_back(std::move(item));
    for (auto &item: right.data) combined.push_back(std::move(item));

    const auto first_size = (combined.size() + 2) / 3;
    const auto second_size = (combined.size() + 1) / 3;
    const auto second_begin = combined.begin() + first_size;
    const auto third_begin = second_begin + second_size;
    left.data.assign(std::make_move_iterator(combined.begin()), std::make_move_iterator(second_begin));
    middle.data.assign(std::make_move_iterator(second_begin), std::make_move_iterator(third_begin));
    right.data.assign(std::make_move_iterator(third_begin), std::make_move_iterator(combined.end()));
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::distribute_middle_pair(
    bsptree_node_middle &left,
    bsptree_node_middle &right) {
    std::vector<storage::page_id_t> combined;
    combined.reserve(left._pointers.size() + right._pointers.size());
    for (const auto pointer: left._pointers) combined.push_back(pointer);
    for (const auto pointer: right._pointers) combined.push_back(pointer);

    const auto left_size = combined.size() / 2;
    left._pointers.assign(combined.begin(), combined.begin() + static_cast<ptrdiff_t>(left_size));
    right._pointers.assign(combined.begin() + static_cast<ptrdiff_t>(left_size), combined.end());
    rebuild_middle_keys(left);
    rebuild_middle_keys(right);
    set_children_parent(left);
    set_children_parent(right);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::distribute_middle_triple_pair(
    bsptree_node_middle &left, bsptree_node_middle &middle, bsptree_node_middle &right) {
    std::vector<storage::page_id_t> combined;
    combined.reserve(left._pointers.size() + middle._pointers.size() + right._pointers.size());
    for (const auto pointer: left._pointers) combined.push_back(pointer);
    for (const auto pointer: middle._pointers) combined.push_back(pointer);
    for (const auto pointer: right._pointers) combined.push_back(pointer);

    const auto left_size = combined.size() / 2;
    left._pointers.assign(combined.begin(), combined.begin() + static_cast<ptrdiff_t>(left_size));
    middle._pointers.assign(combined.begin() + static_cast<ptrdiff_t>(left_size), combined.end());
    rebuild_middle_keys(left);
    rebuild_middle_keys(middle);
    set_children_parent(left);
    set_children_parent(middle);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::split_middle_pair_triple(
    bsptree_node_middle &left, bsptree_node_middle &middle, bsptree_node_middle &right) {
    std::vector<storage::page_id_t> combined;
    combined.reserve(left._pointers.size() + right._pointers.size());
    for (const auto pointer: left._pointers) combined.push_back(pointer);
    for (const auto pointer: right._pointers) combined.push_back(pointer);

    const auto first_size = (combined.size() + 2) / 3;
    const auto second_size = (combined.size() + 1) / 3;
    const auto second_begin = combined.begin() + static_cast<ptrdiff_t>(first_size);
    const auto third_begin = second_begin + static_cast<ptrdiff_t>(second_size);
    left._pointers.assign(combined.begin(), second_begin);
    middle._pointers.assign(second_begin, third_begin);
    right._pointers.assign(third_begin, combined.end());
    rebuild_middle_keys(left);
    rebuild_middle_keys(middle);
    rebuild_middle_keys(right);
    set_children_parent(left);
    set_children_parent(middle);
    set_children_parent(right);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::set_children_parent(const bsptree_node_middle &node) {
    for (const auto pointer: node._pointers) {
        set_node_parent(pointer, node._header._self);
    }
}

// endregion distribution helpers implementations

// region BSP_tree disk helpers implementations
template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::load_header() {
    storage::page_buffer page{};
    _pager.read_page(storage::header_page_id, page);

    storage::PageReader reader(page);
    reader.read_trivial_into(_disk_header._magic, _disk_header._version, _disk_header._tree_order,
                             _disk_header._page_layout, _disk_header._term_capacity,
                             _disk_header._middle_capacity, _disk_header._root_page,
                             _disk_header._first_term, _disk_header._next_page,
                             _disk_header._first_free_page, _disk_header._size);

    if (_disk_header._magic != disk_magic || _disk_header._version != disk_version) {
        throw std::runtime_error("Corrupted BSP_tree disk header");
    }
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::validate_header() const {
    const auto file_size = std::filesystem::file_size(_pager.path());
    if (file_size % storage::page_size != 0) {
        throw std::runtime_error("Corrupted BSP_tree disk file: size is not page-aligned");
    }

    const auto pages_count = file_size / storage::page_size;
    const auto valid_page = [pages_count](const storage::page_id_t page_id) {
        return page_id != storage::invalid_page_id && page_id < pages_count;
    };

    if (!valid_page(_disk_header._root_page) || !valid_page(_disk_header._first_term)) {
        throw std::runtime_error("Corrupted BSP_tree disk header: page id is outside the file");
    }
    if (_disk_header._next_page < storage::first_data_page_id) {
        throw std::runtime_error("Corrupted BSP_tree disk header: next page is invalid");
    }
    if (_disk_header._tree_order != t || _disk_header._page_layout != disk_variable_page_layout ||
        _disk_header._term_capacity != maximum_keys_in_node || _disk_header._middle_capacity != maximum_keys_in_node) {
        throw std::runtime_error("Incompatible BSP_tree disk header: tree parameters do not match");
    }

    const auto root_type = read_page_type(_disk_header._root_page);
    if (root_type != bsptree_page_type::term && root_type != bsptree_page_type::middle) {
        throw std::runtime_error("Corrupted BSP_tree disk header: root page is not a tree node");
    }
    if (read_page_type(_disk_header._first_term) != bsptree_page_type::term) {
        throw std::runtime_error("Corrupted BSP_tree disk header: first term is not a term page");
    }
    if (_disk_header._next_page > pages_count) {
        throw std::runtime_error("Corrupted BSP_tree disk header: next page is outside the file");
    }
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::validate_tree() const {
    const auto file_size = std::filesystem::file_size(_pager.path());
    const auto pages_count = file_size / storage::page_size;
    std::vector visited_pages(pages_count, false);

    const auto subtree_size =
            validate_subtree(_disk_header._root_page, storage::invalid_page_id, true, visited_pages);
    if (subtree_size != _disk_header._size) {
        throw std::runtime_error("Corrupted BSP_tree disk tree: header size does not match tree payload");
    }

    storage::page_id_t term_page = _disk_header._first_term;
    std::vector term_chain_seen(pages_count, false);
    std::uint64_t term_chain_size{};
    std::optional<TKey> previous_key;

    while (term_page != storage::invalid_page_id) {
        if (term_page >= pages_count || term_page == storage::header_page_id) {
            throw std::runtime_error("Corrupted BSP_tree disk term chain: page id is outside the file");
        }
        if (term_chain_seen[term_page]) {
            throw std::runtime_error("Corrupted BSP_tree disk term chain: cycle detected");
        }
        if (!visited_pages[term_page]) {
            throw std::runtime_error("Corrupted BSP_tree disk term chain: term is not reachable from root");
        }

        const auto term = read_term(term_page);
        for (const auto &[key, value]: term.data) {
            if (previous_key && !compare_keys(*previous_key, key)) {
                throw std::runtime_error("Corrupted BSP_tree disk term chain: keys are not strictly increasing");
            }
            previous_key = key;
            ++term_chain_size;
        }

        term_chain_seen[term_page] = true;
        term_page = term._next;
    }

    if (term_chain_size != subtree_size) {
        throw std::runtime_error("Corrupted BSP_tree disk term chain: payload count mismatch");
    }

    auto free_page = _disk_header._first_free_page;
    while (free_page != storage::invalid_page_id) {
        if (free_page >= visited_pages.size()) {
            throw std::runtime_error("Corrupted BSP_tree disk free list: page id is outside the file");
        }

        if (visited_pages[free_page]) {
            throw std::runtime_error("Corrupted BSP_tree disk free list: page is already reachable");
        }
        if (read_page_type(free_page) != bsptree_page_type::free) {
            throw std::runtime_error("Corrupted BSP_tree disk free list: page is not marked free");
        }

        visited_pages[free_page] = true;
        storage::page_buffer buffer{};
        _pager.read_page(free_page, buffer);
        storage::PageReader reader(buffer, sizeof(bsptree_page_type));
        free_page = reader.read_trivial<storage::page_id_t>();
    }
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
std::uint64_t BSP_tree<TKey, TValue, compare, t>::validate_subtree(
    const storage::page_id_t page_id,
    const storage::page_id_t expected_parent,
    const bool is_root,
    std::vector<bool> &reachable_pages) const {
    if (page_id == storage::invalid_page_id || page_id == storage::header_page_id ||
        page_id >= reachable_pages.size()) {
        throw std::runtime_error("Corrupted BSP_tree disk tree: page id is outside the file");
    }

    if (reachable_pages[page_id]) {
        throw std::runtime_error("Corrupted BSP_tree disk tree: page is referenced more than once");
    }
    reachable_pages[page_id] = true;

    const auto node_type = read_page_type(page_id);
    if (node_type == bsptree_page_type::term) {
        const auto term = read_term(page_id);
        if (term._header._self != page_id || term._header._parent != expected_parent) {
            throw std::runtime_error("Corrupted BSP_tree disk term: header page links are inconsistent");
        }

        for (std::size_t i = 1; i < term.data.size(); ++i) {
            if (!compare_keys(term.data[i - 1].first, term.data[i].first)) {
                throw std::runtime_error("Corrupted BSP_tree disk term: keys are not strictly increasing");
            }
        }

        return term.data.size();
    }

    if (node_type != bsptree_page_type::middle) {
        throw std::runtime_error("Corrupted BSP_tree disk tree: reachable page is not a tree node");
    }

    const auto middle = read_middle(page_id);
    if (middle._header._self != page_id || middle._header._parent != expected_parent) {
        throw std::runtime_error("Corrupted BSP_tree disk middle: header page links are inconsistent");
    }
    if (middle._pointers.size() != middle._keys.size() + 1 || middle._pointers.empty()) {
        throw std::runtime_error("Corrupted BSP_tree disk middle: pointer/key counts are inconsistent");
    }
    if (is_root && middle._pointers.size() < 2) {
        throw std::runtime_error("Corrupted BSP_tree disk root: middle root has too few pointers");
    }

    for (std::size_t i = 1; i < middle._keys.size(); ++i) {
        if (!compare_keys(middle._keys[i - 1], middle._keys[i])) {
            throw std::runtime_error("Corrupted BSP_tree disk middle: separator keys are not strictly increasing");
        }
    }
    for (std::size_t i = 1; i < middle._pointers.size(); ++i) {
        if (!keys_equal(middle._keys[i - 1], subtree_first_key(middle._pointers[i]))) {
            throw std::runtime_error("Corrupted BSP_tree disk middle: separator key does not match pointer");
        }
    }

    std::uint64_t subtree_size{};
    for (const auto pointer: middle._pointers) {
        subtree_size += validate_subtree(pointer, page_id, false, reachable_pages);
    }
    return subtree_size;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::write_header() {
    storage::page_buffer page{};
    storage::PageWriter writer(page);
    writer.write_trivial_many(_disk_header._magic, _disk_header._version, _disk_header._tree_order,
                              _disk_header._page_layout, _disk_header._term_capacity,
                              _disk_header._middle_capacity, _disk_header._root_page,
                              _disk_header._first_term, _disk_header._next_page,
                              _disk_header._first_free_page, _disk_header._size);
    _pager.write_page(storage::header_page_id, page);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
storage::page_id_t BSP_tree<TKey, TValue, compare, t>::allocate_page() {
    if (_disk_header._first_free_page != storage::invalid_page_id) {
        const auto page = _disk_header._first_free_page;
        storage::page_buffer buffer{};
        _pager.read_page(page, buffer);
        storage::PageReader reader(buffer, sizeof(bsptree_page_type));
        _disk_header._first_free_page = reader.read_trivial<storage::page_id_t>();
        return page;
    }

    const auto page = _pager.allocate_page();
    if (page >= _disk_header._next_page) {
        _disk_header._next_page = page + 1;
    }
    return page;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::deallocate_page(const storage::page_id_t page_id) {
    storage::page_buffer page{};
    storage::PageWriter writer(page);
    writer.write_trivial_many(bsptree_page_type::free, _disk_header._first_free_page);
    _pager.write_page(page_id, page);
    _disk_header._first_free_page = page_id;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::release_disk_subtree(const storage::page_id_t page_id) {
    if (page_id == storage::invalid_page_id) return;

    const auto type = read_page_type(page_id);
    if (type == bsptree_page_type::free) return;
    if (type == bsptree_page_type::middle) {
        for (const auto pointer: read_middle(page_id)._pointers) release_disk_subtree(pointer);
    }

    deallocate_page(page_id);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::bsptree_page_type BSP_tree<TKey, TValue, compare, t>::read_page_type(
    const storage::page_id_t page_id) const {
    storage::page_buffer page{};
    _pager.read_page(page_id, page);
    storage::PageReader reader(page);
    return reader.read_trivial<bsptree_page_type>();
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::bsptree_node_term BSP_tree<TKey, TValue, compare, t>::read_term(
    const storage::page_id_t page_id) const {
    storage::page_buffer page{};
    _pager.read_page(page_id, page);

    storage::PageReader reader(page);
    bsptree_node_term node;
    node._header._type = reader.read_trivial<bsptree_page_type>();
    reader.read_trivial_into(node._header._keys_count, node._header._self, node._header._parent, node._next);

    if (node._header._type != bsptree_page_type::term || node._header._keys_count > maximum_keys_in_node) {
        throw std::runtime_error("Corrupted BSP_tree disk term");
    }

    node.data.reserve(node._header._keys_count);
    for (std::uint16_t i = 0; i < node._header._keys_count; ++i) {
        auto key_bytes = reader.read_sized_bytes();
        auto value_bytes = reader.read_sized_bytes();
        auto key = storage::Serializer<TKey>::read(key_bytes);
        auto value = storage::Serializer<TValue>::read(value_bytes);
        node.data.emplace_back(std::move(key), std::move(value));
    }

    return node;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::write_term(const storage::page_id_t page_id,
                                                         const bsptree_node_term &node) {
    if (node.data.size() > maximum_keys_in_node) {
        throw std::runtime_error("Disk term node is over capacity");
    }

    storage::page_buffer page{};
    storage::PageWriter writer(page);
    writer.write_trivial_many(bsptree_page_type::term,
                              static_cast<std::uint16_t>(node.data.size()), page_id,
                              node._header._parent, node._next);

    for (const auto &[key, value]: node.data) {
        writer.write_sized_bytes(storage::Serializer<TKey>::to_bytes(key));
        writer.write_sized_bytes(storage::Serializer<TValue>::to_bytes(value));
    }

    _pager.write_page(page_id, page);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::bsptree_node_middle BSP_tree<TKey, TValue, compare, t>::read_middle(
    const storage::page_id_t page_id) const {
    storage::page_buffer page{};
    _pager.read_page(page_id, page);

    storage::PageReader reader(page);
    bsptree_node_middle node;
    node._header._type = reader.read_trivial<bsptree_page_type>();
    reader.read_trivial_into(node._header._keys_count, node._header._self, node._header._parent);

    if (node._header._type != bsptree_page_type::middle || node._header._keys_count > maximum_keys_in_node) {
        throw std::runtime_error("Corrupted BSP_tree disk middle node");
    }

    node._keys.reserve(node._header._keys_count);
    node._pointers.reserve(node._header._keys_count + 1);
    node._pointers.push_back(reader.read_trivial<storage::page_id_t>());
    for (std::uint16_t i = 0; i < node._header._keys_count; ++i) {
        auto key_bytes = reader.read_sized_bytes();
        node._keys.push_back(storage::Serializer<TKey>::read(key_bytes));
        node._pointers.push_back(reader.read_trivial<storage::page_id_t>());
    }

    return node;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::write_middle(const storage::page_id_t page_id,
                                                           const bsptree_node_middle &node) {
    if (node._keys.size() > maximum_keys_in_node || node._pointers.size() != node._keys.size() + 1) {
        throw std::runtime_error("Disk middle node is over capacity or malformed");
    }
    if (node._header._self != page_id) {
        throw std::runtime_error("Disk middle node page id mismatch");
    }

    storage::page_buffer page{};
    storage::PageWriter writer(page);
    writer.write_trivial_many(bsptree_page_type::middle,
                              static_cast<std::uint16_t>(node._keys.size()), node._header._self,
                              node._header._parent, node._pointers.front());
    for (std::size_t i = 0; i < node._keys.size(); ++i) {
        writer.write_sized_bytes(storage::Serializer<TKey>::to_bytes(node._keys[i]));
        writer.write_trivial<storage::page_id_t>(node._pointers[i + 1]);
    }

    _pager.write_page(page_id, page);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
TKey BSP_tree<TKey, TValue, compare, t>::subtree_first_key(const storage::page_id_t page_id) const {
    if (read_page_type(page_id) == bsptree_page_type::term) {
        const auto term = read_term(page_id);
        if (term.data.empty()) throw std::runtime_error("Disk term has no first key");
        return term.data.front().first;
    }

    const auto middle = read_middle(page_id);
    if (middle._pointers.empty()) throw std::runtime_error("Disk middle node has no pointer");
    return subtree_first_key(middle._pointers.front());
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::set_node_parent(const storage::page_id_t page_id,
                                                              const storage::page_id_t parent_page) {
    if (read_page_type(page_id) == bsptree_page_type::term) {
        auto term = read_term(page_id);
        term._header._parent = parent_page;
        write_term(page_id, term);
        return;
    }

    auto middle = read_middle(page_id);
    middle._header._parent = parent_page;
    write_middle(page_id, middle);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::rebuild_separator_keys(const storage::page_id_t page_id) {
    if (read_page_type(page_id) == bsptree_page_type::term) return;

    auto middle = read_middle(page_id);
    for (const auto pointer: middle._pointers) {
        rebuild_separator_keys(pointer);
    }

    wrebuilt_middle(middle);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::rebuild_middle_keys(bsptree_node_middle &node) {
    node._keys.clear();
    for (std::size_t i = 1; i < node._pointers.size(); ++i) {
        node._keys.push_back(subtree_first_key(node._pointers[i]));
    }
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::rebalance_term(const storage::page_id_t term_page,
                                                                         bsptree_node_term &&term) {
    if (term_page == _disk_header._root_page || term.data.size() >= minimum_keys_in_node) {
        write_term(term_page, term);
        return;
    }
    if (term._header._parent == storage::invalid_page_id) throw std::runtime_error("Disk non-root term has no parent");

    auto parent = read_middle(term._header._parent);
    const auto pointer_it = std::ranges::find(parent._pointers, term_page);
    if (pointer_it == parent._pointers.end()) throw std::runtime_error("Corrupted BSP_tree disk term parent");
    const auto pointer_index = pointer_it - parent._pointers.begin();

    if (pointer_index > 0 && read_page_type(parent._pointers[pointer_index - 1]) == bsptree_page_type::term) {
        if (auto left = read_term(parent._pointers[pointer_index - 1]);
            left.data.size() + term.data.size() >= 2 * minimum_keys_in_node) {
            distribute_term_pair(left, term);
            write_term(left._header._self, left);
            write_term(term_page, term);
            wrebuilt_middle(parent);
            return;
        }
    }

    if (pointer_index + 1 < parent._pointers.size() &&
        read_page_type(parent._pointers[pointer_index + 1]) == bsptree_page_type::term) {
        if (auto right = read_term(parent._pointers[pointer_index + 1]);
            term.data.size() + right.data.size() >= 2 * minimum_keys_in_node) {
            distribute_term_pair(term, right);
            write_term(term_page, term);
            write_term(right._header._self, right);
            wrebuilt_middle(parent);
            return;
        }
    }

    if (pointer_index > 0 && pointer_index + 1 < parent._pointers.size() &&
        read_page_type(parent._pointers[pointer_index - 1]) == bsptree_page_type::term &&
        read_page_type(parent._pointers[pointer_index + 1]) == bsptree_page_type::term) {
        auto left = read_term(parent._pointers[pointer_index - 1]);
        if (auto right = read_term(parent._pointers[pointer_index + 1]);
            left.data.size() + term.data.size() + right.data.size() <= 2 * maximum_keys_in_node) {
            distribute_term_triple_pair(left, term, right);
            parent._pointers.erase(parent._pointers.begin() + pointer_index + 1);
            rebuild_middle_keys(parent);
            write_term(left._header._self, left);
            write_term(term_page, term);
            deallocate_page(right._header._self);
            rebalance_middle(parent._header._self, std::move(parent));
            return;
        }
    }

    if (pointer_index + 2 < parent._pointers.size() &&
        read_page_type(parent._pointers[pointer_index + 1]) == bsptree_page_type::term &&
        read_page_type(parent._pointers[pointer_index + 2]) == bsptree_page_type::term) {
        auto right = read_term(parent._pointers[pointer_index + 1]);
        if (auto far_right = read_term(parent._pointers[pointer_index + 2]);
            term.data.size() + right.data.size() + far_right.data.size() <= 2 * maximum_keys_in_node) {
            distribute_term_triple_pair(term, right, far_right);
            parent._pointers.erase(parent._pointers.begin() + pointer_index + 2);
            rebuild_middle_keys(parent);
            write_term(term_page, term);
            write_term(right._header._self, right);
            deallocate_page(far_right._header._self);
            rebalance_middle(parent._header._self, std::move(parent));
            return;
        }
    }

    if (pointer_index > 1 &&
        read_page_type(parent._pointers[pointer_index - 2]) == bsptree_page_type::term &&
        read_page_type(parent._pointers[pointer_index - 1]) == bsptree_page_type::term) {
        auto far_left = read_term(parent._pointers[pointer_index - 2]);
        if (auto left = read_term(parent._pointers[pointer_index - 1]);
            far_left.data.size() + left.data.size() + term.data.size() <= 2 * maximum_keys_in_node) {
            distribute_term_triple_pair(far_left, left, term);
            parent._pointers.erase(parent._pointers.begin() + pointer_index);
            rebuild_middle_keys(parent);
            write_term(far_left._header._self, far_left);
            write_term(left._header._self, left);
            deallocate_page(term_page);
            rebalance_middle(parent._header._self, std::move(parent));
            return;
        }
    }

    if (pointer_index > 0 && read_page_type(parent._pointers[pointer_index - 1]) == bsptree_page_type::term) {
        if (auto left = read_term(parent._pointers[pointer_index - 1]);
            left.data.size() + term.data.size() <= maximum_keys_in_node) {
            left.data.insert(left.data.end(), std::make_move_iterator(term.data.begin()),
                             std::make_move_iterator(term.data.end()));
            left._next = term._next;
            parent._pointers.erase(parent._pointers.begin() + pointer_index);
            rebuild_middle_keys(parent);
            write_term(left._header._self, left);
            deallocate_page(term_page);
            rebalance_middle(parent._header._self, std::move(parent));
            return;
        }
    }

    if (pointer_index + 1 < parent._pointers.size() &&
        read_page_type(parent._pointers[pointer_index + 1]) == bsptree_page_type::term) {
        if (auto right = read_term(parent._pointers[pointer_index + 1]);
            term.data.size() + right.data.size() <= maximum_keys_in_node) {
            term.data.insert(term.data.end(), std::make_move_iterator(right.data.begin()),
                             std::make_move_iterator(right.data.end()));
            term._next = right._next;
            parent._pointers.erase(parent._pointers.begin() + pointer_index + 1);
            rebuild_middle_keys(parent);
            write_term(term_page, term);
            deallocate_page(right._header._self);
            rebalance_middle(parent._header._self, std::move(parent));
            return;
        }
    }

    write_term(term_page, term);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::rebalance_middle(
    const storage::page_id_t middle_page,
    bsptree_node_middle &&node) {
    if (middle_page == _disk_header._root_page || node._keys.size() >= minimum_keys_in_node) {
        write_middle(middle_page, node);
        return;
    }
    if (node._header._parent == storage::invalid_page_id) {
        throw std::runtime_error("Disk non-root middle node has no parent");
    }

    auto parent = read_middle(node._header._parent);
    const auto pointer_it = std::ranges::find(parent._pointers, middle_page);
    if (pointer_it == parent._pointers.end()) throw std::runtime_error("Corrupted BSP_tree disk middle parent");
    const auto pointer_index = pointer_it - parent._pointers.begin();
    if (pointer_index > 0 && read_page_type(parent._pointers[pointer_index - 1]) == bsptree_page_type::middle) {
        if (auto left = read_middle(parent._pointers[pointer_index - 1]);
            left._pointers.size() + node._pointers.size() >= 2 * minimum_pointers_in_node) {
            distribute_middle_pair(left, node);
            write_middle(left._header._self, left);
            write_middle(middle_page, node);
            wrebuilt_middle(parent);
            return;
        }
    }

    if (pointer_index + 1 < parent._pointers.size() &&
        read_page_type(parent._pointers[pointer_index + 1]) == bsptree_page_type::middle) {
        if (auto right = read_middle(parent._pointers[pointer_index + 1]);
            node._pointers.size() + right._pointers.size() >= 2 * minimum_pointers_in_node) {
            distribute_middle_pair(node, right);
            write_middle(middle_page, node);
            write_middle(right._header._self, right);
            wrebuilt_middle(parent);
            return;
        }
    }

    if (pointer_index > 0 && pointer_index + 1 < parent._pointers.size() &&
        read_page_type(parent._pointers[pointer_index - 1]) == bsptree_page_type::middle &&
        read_page_type(parent._pointers[pointer_index + 1]) == bsptree_page_type::middle) {
        auto left = read_middle(parent._pointers[pointer_index - 1]);
        if (auto right = read_middle(parent._pointers[pointer_index + 1]);
            left._pointers.size() + node._pointers.size() + right._pointers.size() <= 2 * maximum_pointers_in_node) {
            distribute_middle_triple_pair(left, node, right);
            parent._pointers.erase(parent._pointers.begin() + pointer_index + 1);
            rebuild_middle_keys(parent);
            write_middle(left._header._self, left);
            write_middle(middle_page, node);
            deallocate_page(right._header._self);
            rebalance_middle(parent._header._self, std::move(parent));
            return;
        }
    }

    if (pointer_index + 2 < parent._pointers.size() &&
        read_page_type(parent._pointers[pointer_index + 1]) == bsptree_page_type::middle &&
        read_page_type(parent._pointers[pointer_index + 2]) == bsptree_page_type::middle) {
        auto right = read_middle(parent._pointers[pointer_index + 1]);
        if (auto far_right = read_middle(parent._pointers[pointer_index + 2]);
            node._pointers.size() + right._pointers.size() + far_right._pointers.size() <= 2 * (
                maximum_pointers_in_node)) {
            distribute_middle_triple_pair(node, right, far_right);
            parent._pointers.erase(parent._pointers.begin() + pointer_index + 2);
            rebuild_middle_keys(parent);
            write_middle(middle_page, node);
            write_middle(right._header._self, right);
            deallocate_page(far_right._header._self);
            rebalance_middle(parent._header._self, std::move(parent));
            return;
        }
    }

    if (pointer_index > 1 &&
        read_page_type(parent._pointers[pointer_index - 2]) == bsptree_page_type::middle &&
        read_page_type(parent._pointers[pointer_index - 1]) == bsptree_page_type::middle) {
        auto far_left = read_middle(parent._pointers[pointer_index - 2]);
        if (auto left = read_middle(parent._pointers[pointer_index - 1]);
            far_left._pointers.size() + left._pointers.size() + node._pointers.size() <= 2 * maximum_pointers_in_node) {
            distribute_middle_triple_pair(far_left, left, node);
            parent._pointers.erase(parent._pointers.begin() + pointer_index);
            rebuild_middle_keys(parent);
            write_middle(far_left._header._self, far_left);
            write_middle(left._header._self, left);
            deallocate_page(middle_page);
            rebalance_middle(parent._header._self, std::move(parent));
            return;
        }
    }

    if (pointer_index > 0 && read_page_type(parent._pointers[pointer_index - 1]) == bsptree_page_type::middle) {
        if (auto left = read_middle(parent._pointers[pointer_index - 1]);
            left._pointers.size() + node._pointers.size() - 1 <= maximum_keys_in_node) {
            for (const auto pointer: node._pointers) {
                left._pointers.push_back(pointer);
                set_node_parent(pointer, left._header._self);
            }
            parent._pointers.erase(parent._pointers.begin() + pointer_index);
            wrebuilt_middle(left);
            rebuild_middle_keys(parent);
            deallocate_page(middle_page);
            rebalance_middle(parent._header._self, std::move(parent));
            return;
        }
    }

    if (pointer_index + 1 < parent._pointers.size() &&
        read_page_type(parent._pointers[pointer_index + 1]) == bsptree_page_type::middle) {
        if (auto right = read_middle(parent._pointers[pointer_index + 1]);
            node._pointers.size() + right._pointers.size() - 1 <= maximum_keys_in_node) {
            for (const auto pointer: right._pointers) {
                node._pointers.push_back(pointer);
                set_node_parent(pointer, node._header._self);
            }
            parent._pointers.erase(parent._pointers.begin() + pointer_index + 1);
            wrebuilt_middle(node);
            rebuild_middle_keys(parent);
            deallocate_page(right._header._self);
            rebalance_middle(parent._header._self, std::move(parent));
            return;
        }
    }

    write_middle(middle_page, node);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::split_root_term(bsptree_node_term &&term) {
    const auto left_page = term._header._self;
    const auto right_page = allocate_page();
    const auto root_page = allocate_page();
    const auto split_at = term.data.size() / 2;

    auto right = make_term(right_page, root_page, term._next);
    right.data.assign(std::make_move_iterator(term.data.begin() + split_at), std::make_move_iterator(term.data.end()));

    term.data.resize(split_at);
    term._header._parent = root_page;
    term._next = right_page;

    auto root = make_middle(root_page, storage::invalid_page_id);
    root._pointers = {left_page, right_page};
    root._keys = {right.data.front().first};

    _disk_header._root_page = root_page;
    _disk_header._first_term = left_page;

    write_term(left_page, term);
    write_term(right_page, right);
    write_middle(root_page, root);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::split_term_node(const storage::page_id_t term_page,
                                                              bsptree_node_term &&term) {
    if (term._header._parent == storage::invalid_page_id) {
        if (term_page == _disk_header._root_page) {
            split_root_term(std::move(term));
            return;
        }
        throw std::runtime_error("Disk non-root term has no parent");
    }

    auto parent = read_middle(term._header._parent);
    const auto pointer_it = std::ranges::find(parent._pointers, term_page);
    if (pointer_it == parent._pointers.end()) throw std::runtime_error("Corrupted BSP_tree disk parent");

    const auto pointer_index = pointer_it - parent._pointers.begin();

    if (pointer_index > 0 && read_page_type(parent._pointers[pointer_index - 1]) == bsptree_page_type::term) {
        if (auto left = read_term(parent._pointers[pointer_index - 1]);
            left.data.size() + term.data.size() <= 2 * maximum_keys_in_node) {
            distribute_term_pair(left, term);
            write_term(left._header._self, left);
            write_term(term_page, term);
            wrebuilt_middle(parent);
            return;
        }
    }

    if (pointer_index + 1 < parent._pointers.size() &&
        read_page_type(parent._pointers[pointer_index + 1]) == bsptree_page_type::term) {
        if (auto right = read_term(parent._pointers[pointer_index + 1]);
            term.data.size() + right.data.size() <= 2 * maximum_keys_in_node) {
            distribute_term_pair(term, right);
            write_term(term_page, term);
            write_term(right._header._self, right);
            wrebuilt_middle(parent);
            return;
        }
    }

    const auto new_page = allocate_page();

    if (pointer_index > 0 && read_page_type(parent._pointers[pointer_index - 1]) == bsptree_page_type::term) {
        auto left = read_term(parent._pointers[pointer_index - 1]);

        auto middle = make_term(new_page, parent._header._self, term_page);
        split_term_pair_triple(left, middle, term);
        left._next = new_page;

        parent._pointers.insert(parent._pointers.begin() + pointer_index, new_page);
        write_term(left._header._self, left);
        write_term(new_page, middle);
        write_term(term_page, term);
    } else if (pointer_index + 1 < parent._pointers.size() &&
               read_page_type(parent._pointers[pointer_index + 1]) == bsptree_page_type::term) {
        auto right = read_term(parent._pointers[pointer_index + 1]);

        auto middle = make_term(new_page, parent._header._self, right._header._self);
        split_term_pair_triple(term, middle, right);
        term._next = new_page;

        parent._pointers.insert(parent._pointers.begin() + pointer_index + 1, new_page);
        write_term(term_page, term);
        write_term(new_page, middle);
        write_term(right._header._self, right);
    } else {
        deallocate_page(new_page);
        throw std::runtime_error("Disk term split requires a term sibling");
    }

    rebuild_middle_keys(parent);
    if (parent._keys.size() > maximum_keys_in_node) {
        split_middle_node(parent._header._self, std::move(parent));
    } else {
        write_middle(parent._header._self, parent);
    }
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::split_root_middle(bsptree_node_middle &&root) {
    if (root._header._self != _disk_header._root_page || root._pointers.size() <= 2) {
        throw std::runtime_error("Invalid disk root middle split");
    }

    const auto left_page = root._header._self;
    const auto right_page = allocate_page();
    const auto new_root_page = allocate_page();
    const auto split_at = root._pointers.size() / 2;

    auto left = make_middle(left_page, new_root_page);
    left._pointers.assign(root._pointers.begin(), root._pointers.begin() + split_at);

    auto right = make_middle(right_page, new_root_page);
    right._pointers.assign(root._pointers.begin() + split_at, root._pointers.end());

    rebuild_middle_keys(left);
    rebuild_middle_keys(right);

    auto new_root = make_middle(new_root_page, storage::invalid_page_id);
    new_root._pointers = {left_page, right_page};
    new_root._keys = {subtree_first_key(right._pointers.front())};

    _disk_header._root_page = new_root_page;

    write_middle(left_page, left);
    write_middle(right_page, right);
    write_middle(new_root_page, new_root);

    set_node_parent(left_page, new_root_page);
    set_node_parent(right_page, new_root_page);
    for (const auto pointer: left._pointers) {
        set_node_parent(pointer, left_page);
    }
    for (const auto pointer: right._pointers) {
        set_node_parent(pointer, right_page);
    }
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::split_middle_node(const storage::page_id_t middle_page,
                                                                bsptree_node_middle &&node) {
    if (node._header._self != middle_page) throw std::runtime_error("Disk middle split page id mismatch");
    if (middle_page == _disk_header._root_page) {
        split_root_middle(std::move(node));
        return;
    }
    if (node._header._parent == storage::invalid_page_id) {
        throw std::runtime_error("Disk non-root middle node has no parent");
    }

    auto parent = read_middle(node._header._parent);
    const auto pointer_it = std::ranges::find(parent._pointers, middle_page);
    if (pointer_it == parent._pointers.end()) throw std::runtime_error("Corrupted BSP_tree disk middle parent");
    const auto pointer_index = pointer_it - parent._pointers.begin();

    if (pointer_index > 0 && read_page_type(parent._pointers[pointer_index - 1]) == bsptree_page_type::middle) {
        if (auto left = read_middle(parent._pointers[pointer_index - 1]);
            left._pointers.size() + node._pointers.size() <= 2 * maximum_pointers_in_node) {
            distribute_middle_pair(left, node);
            write_middle(left._header._self, left);
            write_middle(middle_page, node);
            wrebuilt_middle(parent);
            return;
        }
    }

    if (pointer_index + 1 < parent._pointers.size() &&
        read_page_type(parent._pointers[pointer_index + 1]) == bsptree_page_type::middle) {
        if (auto right = read_middle(parent._pointers[pointer_index + 1]);
            node._pointers.size() + right._pointers.size() <= 2 * maximum_pointers_in_node) {
            distribute_middle_pair(node, right);
            write_middle(middle_page, node);
            write_middle(right._header._self, right);
            wrebuilt_middle(parent);
            return;
        }
    }

    const auto new_page = allocate_page();

    if (pointer_index > 0 && read_page_type(parent._pointers[pointer_index - 1]) == bsptree_page_type::middle) {
        auto left = read_middle(parent._pointers[pointer_index - 1]);

        auto middle = make_middle(new_page, parent._header._self);
        split_middle_pair_triple(left, middle, node);

        parent._pointers.insert(parent._pointers.begin() + pointer_index, new_page);
        write_middle(left._header._self, left);
        write_middle(new_page, middle);
        write_middle(middle_page, node);
    } else if (pointer_index + 1 < parent._pointers.size() && read_page_type(parent._pointers[pointer_index + 1])
               == bsptree_page_type::middle) {
        auto right = read_middle(parent._pointers[pointer_index + 1]);

        auto middle = make_middle(new_page, parent._header._self);
        split_middle_pair_triple(node, middle, right);

        parent._pointers.insert(parent._pointers.begin() + (pointer_index + 1), new_page);
        write_middle(middle_page, node);
        write_middle(new_page, middle);
        write_middle(right._header._self, right);
    } else {
        deallocate_page(new_page);
        throw std::runtime_error("Disk middle split requires a middle sibling");
    }

    rebuild_middle_keys(parent);
    if (parent._keys.size() > maximum_keys_in_node) {
        split_middle_node(parent._header._self, std::move(parent));
    } else {
        write_middle(parent._header._self, parent);
    }
}

// endregion BSP_tree disk helpers implementations
