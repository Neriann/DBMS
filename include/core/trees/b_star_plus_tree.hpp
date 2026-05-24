#pragma once
#include <iterator>
#include <utility>
#include <vector>
#include <boost/container/static_vector.hpp>
#include <stack>
#include <pp_allocator.h>
#include <associative_container.h>
#include <initializer_list>
#include <optional>

template<typename TKey, typename TValue, comparator<TKey> compare = std::less<TKey>, std::size_t t = 5>
class BSP_tree final : compare {
public:
    using tree_data_type = std::pair<TKey, TValue>;
    using tree_data_type_const = std::pair<const TKey, TValue>;
    using value_type = tree_data_type_const;

private:
    static constexpr size_t minimum_keys_in_root = 1;
    static constexpr size_t maximum_keys_in_root = 4 * t - 1;

    static constexpr size_t minimum_keys_in_node = 2 * t - 1;
    static constexpr size_t maximum_keys_in_node = 3 * t - 1;

    // region comparators declaration

    bool compare_keys(const TKey &lhs, const TKey &rhs) const;

    bool compare_pairs(const tree_data_type &lhs, const tree_data_type &rhs) const;

    // endregion comparators declaration

    struct BSPTreeNodeBase {
        bool _is_terminated;

        BSPTreeNodeBase() noexcept;

        virtual ~BSPTreeNodeBase() = default;
    };

    struct BSPTreeNodeTerm : BSPTreeNodeBase {
        BSPTreeNodeTerm *_next;
        boost::container::static_vector<tree_data_type, maximum_keys_in_root + 1> _data;

        BSPTreeNodeTerm() noexcept;
    };

    struct BSPTreeNodeMiddle : BSPTreeNodeBase {
        boost::container::static_vector<TKey, maximum_keys_in_root + 1> _keys;
        boost::container::static_vector<BSPTreeNodeBase *, maximum_keys_in_root + 2> _pointers;

        BSPTreeNodeMiddle() noexcept;
    };

    pp_allocator<value_type> _allocator;
    BSPTreeNodeBase *_root;
    size_t _size;

    pp_allocator<value_type> get_allocator() const noexcept;

public:
    // region constructors declaration

    explicit BSP_tree(const compare &cmp = compare(), pp_allocator<value_type> = pp_allocator<value_type>());

    explicit BSP_tree(pp_allocator<value_type> alloc, const compare &cmp = compare());

    template<input_iterator_for_pair<TKey, TValue> iterator>
    explicit BSP_tree(iterator begin, iterator end, const compare &cmp = compare(),
                      pp_allocator<value_type> = pp_allocator<value_type>());

    BSP_tree(std::initializer_list<std::pair<TKey, TValue> > data, const compare &cmp = compare(),
             pp_allocator<value_type> = pp_allocator<value_type>());

    // endregion constructors declaration

    // region five declaration

    BSP_tree(const BSP_tree &other);

    BSP_tree(BSP_tree &&other) noexcept;

    BSP_tree &operator=(const BSP_tree &other);

    BSP_tree &operator=(BSP_tree &&other) noexcept;

    ~BSP_tree() noexcept;

    // endregion five declaration

    // region iterators declaration

    class BSPTreeIterator;
    class BSPTreeConstIterator;

    class BSPTreeIterator final {
        BSPTreeNodeTerm *_node;
        size_t _index;

    public:
        using value_type = tree_data_type_const;
        using reference = value_type &;
        using pointer = value_type *;
        using iterator_category = std::forward_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = BSPTreeIterator;

        friend class BSP_tree;
        friend class BSPTreeConstIterator;

        reference operator*() const noexcept;

        pointer operator->() const noexcept;

        self &operator++();

        self operator++(int);

        bool operator==(const self &other) const noexcept;

        bool operator!=(const self &other) const noexcept;

        [[nodiscard]] size_t current_node_keys_count() const noexcept;

        [[nodiscard]] size_t index() const noexcept;

        explicit BSPTreeIterator(BSPTreeNodeTerm *node = nullptr, size_t index = 0);
    };

    class BSPTreeConstIterator final {
        const BSPTreeNodeTerm *_node;
        size_t _index;

    public:
        using value_type = tree_data_type_const;
        using reference = const value_type &;
        using pointer = const value_type *;
        using iterator_category = std::forward_iterator_tag;
        using difference_type = ptrdiff_t;
        using self = BSPTreeConstIterator;

        friend class BSP_tree;
        friend class BSPTreeIterator;

        explicit BSPTreeConstIterator(const BSPTreeIterator &it) noexcept;

        reference operator*() const noexcept;

        pointer operator->() const noexcept;

        self &operator++();

        self operator++(int);

        bool operator==(const self &other) const noexcept;

        bool operator!=(const self &other) const noexcept;

        [[nodiscard]] size_t current_node_keys_count() const noexcept;

        [[nodiscard]] size_t index() const noexcept;

        explicit BSPTreeConstIterator(const BSPTreeNodeTerm *node = nullptr, size_t index = 0);
    };

    friend class btree_iterator;
    friend class btree_const_iterator;

    // endregion iterators declaration

    // region element access declaration

    /*
     * Returns a reference to the mapped value of the element with specified key. If no such element exists, an exception of type std::out_of_range is thrown.
     */
    TValue &at(const TKey &);

    const TValue &at(const TKey &) const;

    /*
     * If key not exists, makes default initialization of value
     */
    TValue &operator[](const TKey &key);

    TValue &operator[](TKey &&key);

    // endregion element access declaration
    // region iterator begins declaration

    BSPTreeIterator begin();

    BSPTreeIterator end();

    BSPTreeConstIterator begin() const;

    BSPTreeConstIterator end() const;

    BSPTreeConstIterator cbegin() const;

    BSPTreeConstIterator cend() const;

    // endregion iterator begins declaration

    // region lookup declaration

    [[nodiscard]] size_t size() const noexcept;

    [[nodiscard]] bool empty() const noexcept;

    /*
     * Returns end() if not exist
     */

    BSPTreeIterator find(const TKey &key);

    BSPTreeConstIterator find(const TKey &key) const;

    BSPTreeIterator lower_bound(const TKey &key);

    BSPTreeConstIterator lower_bound(const TKey &key) const;

    BSPTreeIterator upper_bound(const TKey &key);

    BSPTreeConstIterator upper_bound(const TKey &key) const;

    bool contains(const TKey &key) const;

    // endregion lookup declaration

    // region modifiers declaration

    void clear() noexcept;

    /*
     * Does nothing if key exists, delegates to emplace.
     * Second return value is true, when inserted
     */
    std::pair<BSPTreeIterator, bool> insert(const tree_data_type &data);

    std::pair<BSPTreeIterator, bool> insert(tree_data_type &&data);

    template<typename... Args>
    std::pair<BSPTreeIterator, bool> emplace(Args &&... args);

    /*
     * Updates value if key exists, delegates to emplace.
     */
    BSPTreeIterator insert_or_assign(const tree_data_type &data);

    BSPTreeIterator insert_or_assign(tree_data_type &&data);

    template<typename... Args>
    BSPTreeIterator emplace_or_assign(Args &&... args);

    /*
     * Return iterator to node next ro removed or end() if key not exists
     */
    BSPTreeIterator erase(BSPTreeIterator pos);

    BSPTreeIterator erase(BSPTreeConstIterator pos);

    BSPTreeIterator erase(BSPTreeIterator beg, BSPTreeIterator en);

    BSPTreeIterator erase(BSPTreeConstIterator beg, BSPTreeConstIterator en);

    BSPTreeIterator erase(const TKey &key);

    // endregion modifiers declaration

    // region helpers declaration
private:
    size_t key_index(BSPTreeNodeMiddle *node, const TKey &key) const;

    size_t key_index(BSPTreeNodeTerm *node, const TKey &key, bool strict) const;

    void merge_nodes(BSPTreeNodeMiddle *parent, size_t i);

    void erase_node(const BSPTreeNodeMiddle *node, const TKey &k);

    static std::pair<BSPTreeNodeMiddle *, size_t> find_parent_recurse(BSPTreeNodeBase *root, BSPTreeNodeBase *child);

    std::pair<BSPTreeNodeMiddle *, size_t> find_parent_of(BSPTreeNodeBase *child) const;

    bool borrow_leaf_right(BSPTreeNodeMiddle *parent, size_t i);

    bool borrow_leaf_left(BSPTreeNodeMiddle *parent, size_t i);

    bool borrow_internal_right(BSPTreeNodeMiddle *parent, size_t i);

    bool borrow_internal_left(BSPTreeNodeMiddle *parent, size_t i);

    void shrink_root_if_needed();

    size_t node_size(BSPTreeNodeBase *n) const noexcept;

    BSPTreeIterator bound(const TKey &key, bool strict = false);

    bool keys_equal(const TKey &a, const TKey &b) const noexcept;

    void insert_into_middle(BSPTreeNodeMiddle *parent, size_t child_idx, TKey sep, BSPTreeNodeBase *right_child);

    void maybe_split_root();

    void split_leaf_child(BSPTreeNodeMiddle *parent, size_t i);

    void split_internal_child(BSPTreeNodeMiddle *parent, size_t i);

    void insert_nonfull(BSPTreeNodeBase *x, tree_data_type &&data);

    void relink_leaves();

    void rebuild_separator_keys();

    void rebuild_separator_keys_recursive(BSPTreeNodeBase *node);

    TKey subtree_first_key(BSPTreeNodeBase *node) const;

    static void collect_leaves_inorder(BSPTreeNodeBase *n, std::vector<BSPTreeNodeTerm *> &out);

    // endregion helpers declaration
};

template<std::input_iterator iterator, comparator<typename std::iterator_traits<iterator>::value_type::first_type>
    compare = std::less<typename std::iterator_traits<iterator>::value_type::first_type>, std::size_t t = 5, typename U>
BSP_tree(iterator begin, iterator end, const compare &cmp = compare(),
         pp_allocator<U> = pp_allocator<U>()) -> BSP_tree<typename std::iterator_traits<
    iterator>::value_type::first_type, typename std::iterator_traits<iterator>::value_type::second_type, compare, t>;

template<typename TKey, typename TValue, comparator<TKey> compare = std::less<TKey>, std::size_t t = 5, typename U>
BSP_tree(std::initializer_list<std::pair<TKey, TValue> > data, const compare &cmp = compare(),
         pp_allocator<U> = pp_allocator<U>()) -> BSP_tree<TKey, TValue, compare, t>;

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
bool BSP_tree<TKey, TValue, compare, t>::compare_pairs(const tree_data_type &lhs, const tree_data_type &rhs) const {
    return compare_keys(lhs.first, rhs.first);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
bool BSP_tree<TKey, TValue, compare, t>::compare_keys(const TKey &lhs, const TKey &rhs) const {
    return compare::operator()(lhs, rhs);
}

// region BSPTreeNodeBase implementation

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeNodeBase::BSPTreeNodeBase() noexcept : _is_terminated(false) {
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeNodeTerm::BSPTreeNodeTerm() noexcept : _next(nullptr) {
    this->_is_terminated = true;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeNodeMiddle::BSPTreeNodeMiddle() noexcept = default;

// region BSP_tree constructor implementations

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
pp_allocator<typename BSP_tree<TKey, TValue, compare, t>::value_type> BSP_tree<TKey, TValue, compare,
    t>::get_allocator() const noexcept {
    return _allocator;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeConstIterator::BSPTreeConstIterator(
    const BSPTreeNodeTerm *node, const size_t index) : _node(node), _index(index) {
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSP_tree(const compare &cmp, pp_allocator<value_type> alloc) : compare(cmp),
    _allocator(alloc), _root(nullptr), _size(0) {
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare,
    t>::BSP_tree(pp_allocator<value_type> alloc, const compare &cmp) : BSP_tree(cmp, alloc) {
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
template<input_iterator_for_pair<TKey, TValue> iterator>
BSP_tree<TKey, TValue, compare, t>::BSP_tree(iterator begin, iterator end, const compare &cmp,
                                             pp_allocator<value_type> alloc) : BSP_tree(cmp, alloc) {
    for (; begin != end; ++begin) insert(*begin);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSP_tree(std::initializer_list<std::pair<TKey, TValue> > data, const compare &cmp,
                                             pp_allocator<value_type> alloc) : BSP_tree(
    data.begin(), data.end(), cmp, alloc) {
}

// endregion BSP_tree constructor implementations

// region BSP_tree copy and move constructors

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSP_tree(const BSP_tree &other) : compare(static_cast<const compare &>(other)),
                                                                      _allocator(other._allocator), _root(nullptr),
                                                                      _size(0) {
    try {
        for (auto it = other.cbegin(); it != other.cend(); ++it) insert({it->first, it->second});
    } catch (...) {
        clear();
        throw std::logic_error("Err while copy construct!");
    }
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare,
    t>::BSP_tree(BSP_tree &&other) noexcept : compare(std::move(static_cast<compare &>(other))),
                                              _allocator(std::move(other._allocator)),
                                              _root(std::exchange(other._root, nullptr)),
                                              _size(std::exchange(other._size, 0)) {
}

// endregion BSP_tree copy and move constructors

// region BSP_tree copy and move assignment operators

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t> &BSP_tree<TKey, TValue, compare, t>::operator=(const BSP_tree &other) {
    if (this == &other) return *this;

    BSP_tree tmp(other);
    *this = std::move(tmp);
    return *this;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t> &BSP_tree<TKey, TValue, compare, t>::operator=(BSP_tree &&other) noexcept {
    if (this == &other) return *this;
    BSP_tree tmp(std::move(other));
    std::swap(static_cast<compare &>(*this), static_cast<compare &>(tmp));
    std::swap(_allocator, tmp._allocator);
    std::swap(_root, tmp._root);
    std::swap(_size, tmp._size);
    return *this;
}

// endregion BSP_tree copy and move assignment operators

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::~BSP_tree() noexcept {
    clear();
}

// region BSP_tree iterators implementations

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare,
    t>::BSPTreeIterator::BSPTreeIterator(BSPTreeNodeTerm *node, const size_t index) : _node(node), _index(index) {
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeIterator::reference BSP_tree<TKey, TValue, compare,
    t>::BSPTreeIterator::operator*() const noexcept {
    return *reinterpret_cast<pointer>(&_node->_data[_index]);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeIterator::pointer BSP_tree<TKey, TValue, compare,
    t>::BSPTreeIterator::operator->() const noexcept {
    return &operator*();
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeIterator &BSP_tree<TKey, TValue, compare, t>::BSPTreeIterator::operator++() {
    if (!_node) return *this;

    if (_index + 1 < _node->_data.size()) {
        ++_index;
        return *this;
    }
    _node = _node->_next;
    _index = 0;
    return *this;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeIterator BSP_tree<TKey, TValue, compare, t>::BSPTreeIterator::operator
++(int) {
    self tmp = *this;
    ++*this;
    return tmp;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
bool BSP_tree<TKey, TValue, compare, t>::BSPTreeIterator::operator==(const self &other) const noexcept {
    return _node == other._node && _index == other._index;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
bool BSP_tree<TKey, TValue, compare, t>::BSPTreeIterator::operator!=(const self &other) const noexcept {
    return !(*this == other);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
size_t BSP_tree<TKey, TValue, compare, t>::BSPTreeIterator::current_node_keys_count() const noexcept {
    return _node ? _node->_data.size() : 0;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
size_t BSP_tree<TKey, TValue, compare, t>::BSPTreeIterator::index() const noexcept {
    return _index;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare,
    t>::BSPTreeConstIterator::BSPTreeConstIterator(const BSPTreeIterator &it) noexcept : _node(it._node),
    _index(it._index) {
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeConstIterator::reference BSP_tree<TKey, TValue, compare,
    t>::BSPTreeConstIterator::operator*() const noexcept {
    return *reinterpret_cast<pointer>(&_node->_data[_index]);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeConstIterator::pointer BSP_tree<TKey, TValue, compare,
    t>::BSPTreeConstIterator::operator->() const noexcept {
    return &operator*();
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeConstIterator &BSP_tree<TKey, TValue, compare,
    t>::BSPTreeConstIterator::operator++() {
    if (!_node) return *this;

    if (_index + 1 < _node->_data.size()) {
        ++_index;
        return *this;
    }
    _node = _node->_next;
    _index = 0;
    return *this;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeConstIterator BSP_tree<TKey, TValue, compare,
    t>::BSPTreeConstIterator::operator++(int) {
    self tmp = *this;
    ++*this;
    return tmp;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
bool BSP_tree<TKey, TValue, compare, t>::BSPTreeConstIterator::operator==(const self &other) const noexcept {
    return _node == other._node && _index == other._index;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
bool BSP_tree<TKey, TValue, compare, t>::BSPTreeConstIterator::operator!=(const self &other) const noexcept {
    return !(*this == other);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
size_t BSP_tree<TKey, TValue, compare, t>::BSPTreeConstIterator::current_node_keys_count() const noexcept {
    return _node ? _node->_data.size() : 0;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
size_t BSP_tree<TKey, TValue, compare, t>::BSPTreeConstIterator::index() const noexcept {
    return _index;
}

// endregion BSP_tree iterators implementations

// region BSP_tree element access implementations

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
TValue &BSP_tree<TKey, TValue, compare, t>::at(const TKey &key) {
    auto it = find(key);
    if (it == end()) throw std::out_of_range("key not found");
    return it->second;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
const TValue &BSP_tree<TKey, TValue, compare, t>::at(const TKey &key) const {
    return const_cast<BSP_tree *>(this)->at(key);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
TValue &BSP_tree<TKey, TValue, compare, t>::operator[](const TKey &key) {
    auto it = find(key);
    if (it == end()) it = insert({key, TValue{}}).first;
    return it->second;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
TValue &BSP_tree<TKey, TValue, compare, t>::operator[](TKey &&key) {
    auto it = find(key);
    if (it == end()) it = insert({std::move(key), TValue{}}).first;
    return it->second;
}

// endregion BSP_tree element access implementations

// region BSP_tree iterator begins implementations

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeIterator BSP_tree<TKey, TValue, compare, t>::begin() {
    if (!_root) return end();

    auto current = _root;
    while (!current->_is_terminated) {
        current = static_cast<BSPTreeNodeMiddle *>(current)->_pointers.front();
    }
    auto leaf = static_cast<BSPTreeNodeTerm *>(current);
    if (leaf->_data.empty()) return end();
    return BSPTreeIterator(leaf, 0);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeIterator BSP_tree<TKey, TValue, compare, t>::end() {
    return BSPTreeIterator();
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeConstIterator BSP_tree<TKey, TValue, compare, t>::begin() const {
    return BSPTreeConstIterator(const_cast<BSP_tree *>(this)->begin());
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeConstIterator BSP_tree<TKey, TValue, compare, t>::end() const {
    return BSPTreeConstIterator();
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeConstIterator BSP_tree<TKey, TValue, compare, t>::cbegin() const {
    return BSPTreeConstIterator(const_cast<BSP_tree *>(this)->begin());
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeConstIterator BSP_tree<TKey, TValue, compare, t>::cend() const {
    return BSPTreeConstIterator(const_cast<BSP_tree *>(this)->end());
}

// endregion BSP_tree iterator begins implementations

// region BSP_tree lookup implementations

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
size_t BSP_tree<TKey, TValue, compare, t>::size() const noexcept {
    return _size;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
bool BSP_tree<TKey, TValue, compare, t>::empty() const noexcept {
    return _size == 0;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeIterator BSP_tree<TKey, TValue, compare, t>::find(const TKey &key) {
    auto it = bound(key);
    return it != end() && !compare_keys(key, it->first) && !compare_keys(it->first, key) ? it : end();
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeConstIterator BSP_tree<TKey, TValue, compare,
    t>::find(const TKey &key) const {
    return BSPTreeConstIterator(const_cast<BSP_tree *>(this)->find(key));
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeIterator BSP_tree<TKey, TValue, compare, t>::lower_bound(const TKey &key) {
    return bound(key, false);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeConstIterator BSP_tree<TKey, TValue, compare, t>::lower_bound(
    const TKey &key) const {
    return BSPTreeConstIterator(const_cast<BSP_tree *>(this)->lower_bound(key));
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeIterator BSP_tree<TKey, TValue, compare, t>::upper_bound(const TKey &key) {
    return bound(key, true);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeConstIterator BSP_tree<TKey, TValue, compare, t>::upper_bound(
    const TKey &key) const {
    return BSPTreeConstIterator(const_cast<BSP_tree *>(this)->upper_bound(key));
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
bool BSP_tree<TKey, TValue, compare, t>::contains(const TKey &key) const {
    return find(key) != end();
}

// endregion BSP_tree lookup implementations

// region BSP_tree modifiers implementations

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::clear() noexcept {
    if (!_root) return;

    std::stack<BSPTreeNodeBase *> nodes;
    nodes.push(_root);

    while (!nodes.empty()) {
        auto current = nodes.top();
        nodes.pop();

        if (!current->_is_terminated)
            for (auto child: static_cast<BSPTreeNodeMiddle *>(current)->_pointers) {
                if (child) nodes.push(child);
            }

        if (current->_is_terminated) {
            _allocator.template delete_object<BSPTreeNodeTerm>(static_cast<BSPTreeNodeTerm *>(current));
        } else {
            _allocator.template delete_object<BSPTreeNodeMiddle>(static_cast<BSPTreeNodeMiddle *>(current));
        }
    }

    _root = nullptr;
    _size = 0;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
std::pair<typename BSP_tree<TKey, TValue, compare, t>::BSPTreeIterator, bool> BSP_tree<TKey, TValue, compare,
    t>::insert(const tree_data_type &data) {
    return insert(tree_data_type(data));
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
std::pair<typename BSP_tree<TKey, TValue, compare, t>::BSPTreeIterator, bool> BSP_tree<TKey, TValue, compare,
    t>::insert(tree_data_type &&data) {
    using leaf_t = BSPTreeNodeTerm;
    const auto k_copy = data.first;

    if (!_root) {
        leaf_t *leaf = _allocator.template new_object<leaf_t>();
        leaf->_data.push_back(std::move(data));
        _root = leaf;
        ++_size;
        return {BSPTreeIterator(leaf, 0), true};
    }

    const auto existing = find(k_copy);
    if (existing != end()) {
        return {existing, false};
    }

    insert_nonfull(_root, std::move(data));
    ++_size;

    maybe_split_root();
    rebuild_separator_keys();
    relink_leaves();
    return {find(k_copy), true};
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
template<typename... Args>
std::pair<typename BSP_tree<TKey, TValue, compare, t>::BSPTreeIterator, bool> BSP_tree<TKey, TValue, compare,
    t>::emplace(Args &&... args) {
    return insert(tree_data_type(std::forward<Args>(args)...));
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeIterator BSP_tree<TKey, TValue, compare, t>::insert_or_assign(
    const tree_data_type &data) {
    return insert_or_assign(tree_data_type(data));
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeIterator BSP_tree<TKey, TValue, compare, t>::insert_or_assign(
    tree_data_type &&data) {
    auto it = find(data.first);
    return it != end() ? (it->second = std::move(data.second), it) : insert(std::move(data)).first;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
template<typename... Args>
BSP_tree<TKey, TValue, compare, t>::BSPTreeIterator BSP_tree<TKey, TValue, compare,
    t>::emplace_or_assign(Args &&... args) {
    return insert_or_assign(tree_data_type(std::forward<Args>(args)...));
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeIterator BSP_tree<TKey, TValue, compare, t>::erase(BSPTreeIterator pos) {
    if (pos == end()) return end();
    return erase(pos->first);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeIterator BSP_tree<TKey, TValue, compare,
    t>::erase(BSPTreeConstIterator pos) {
    if (pos == cend()) return end();
    return erase(pos->first);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeIterator BSP_tree<TKey, TValue, compare, t>::erase(
    BSPTreeIterator beg, BSPTreeIterator en) {
    if (beg == en) return beg;

    const bool has_end = en != this->end();
    const TKey end_value = has_end ? en->first : TKey{};

    while (beg != this->end()) {
        if (has_end && !compare_keys(beg->first, end_value)) break;

        beg = erase(beg);
    }

    return has_end ? lower_bound(end_value) : this->end();
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeIterator BSP_tree<TKey, TValue, compare, t>::erase(
    BSPTreeConstIterator beg, BSPTreeConstIterator en) {
    if (beg == en || beg == cend()) return end();
    return erase(lower_bound(beg->first), en == cend() ? end() : lower_bound(en->first));
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeIterator BSP_tree<TKey, TValue, compare, t>::erase(const TKey &key) {
    if (!_root) {
        return end();
    }

    const auto it = find(key);
    if (it == end()) {
        return end();
    }

    std::optional<TKey> next_key;
    const auto ub = upper_bound(key);
    if (ub != end()) {
        next_key = ub->first;
    }

    erase_node(nullptr, key);
    relink_leaves();

    if (!next_key) {
        return end();
    }
    return lower_bound(*next_key);
}

// endregion BSP_tree modifiers implementations

// region BSP_tree helpers implementations

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::rebuild_separator_keys() {
    if (!_root || _root->_is_terminated) {
        return;
    }

    rebuild_separator_keys_recursive(_root);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::rebuild_separator_keys_recursive(BSPTreeNodeBase *node) {
    if (!node || node->_is_terminated) return;

    auto *middle = static_cast<BSPTreeNodeMiddle *>(node);

    for (auto *child: middle->_pointers) {
        rebuild_separator_keys_recursive(child);
    }

    middle->_keys.clear();

    for (size_t i = 1; i < middle->_pointers.size(); ++i) {
        middle->_keys.push_back(subtree_first_key(middle->_pointers[i]));
    }
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
TKey BSP_tree<TKey, TValue, compare, t>::subtree_first_key(BSPTreeNodeBase *node) const {
    while (!node->_is_terminated) {
        node = static_cast<BSPTreeNodeMiddle *>(node)->_pointers.front();
    }

    return static_cast<BSPTreeNodeTerm *>(node)->_data.front().first;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
size_t BSP_tree<TKey, TValue, compare, t>::key_index(BSPTreeNodeMiddle *node, const TKey &key) const {
    size_t lo = 0;
    size_t hi = node->_keys.size();

    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        if (!compare_keys(key, node->_keys[mid])) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }

    return lo;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
size_t BSP_tree<TKey, TValue, compare, t>::key_index(BSPTreeNodeTerm *node, const TKey &key, bool strict) const {
    size_t lo = 0;
    size_t hi = node->_data.size();

    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;

        if (const auto mid_key = node->_data[mid].first; strict
                                                             ? !compare_keys(key, mid_key)
                                                             : compare_keys(mid_key, key)) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }

    return lo;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::merge_nodes(BSPTreeNodeMiddle *parent, size_t i) {
    auto left_base = parent->_pointers[i];
    auto right_base = parent->_pointers[i + 1];

    if (left_base->_is_terminated) {
        auto left = static_cast<BSPTreeNodeTerm *>(left_base);
        auto right = static_cast<BSPTreeNodeTerm *>(right_base);

        for (auto &item: right->_data) left->_data.push_back(std::move(item));
        left->_next = right->_next;

        _allocator.template delete_object<BSPTreeNodeTerm>(right);
    } else {
        auto left = static_cast<BSPTreeNodeMiddle *>(left_base);
        auto right = static_cast<BSPTreeNodeMiddle *>(right_base);

        left->_keys.push_back(std::move(parent->_keys[i]));
        for (auto &key: right->_keys) left->_keys.push_back(std::move(key));
        for (auto ptr: right->_pointers) left->_pointers.push_back(ptr);

        _allocator.template delete_object<BSPTreeNodeMiddle>(right);
    }

    parent->_keys.erase(parent->_keys.begin() + i);
    parent->_pointers.erase(parent->_pointers.begin() + i + 1);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
std::pair<typename BSP_tree<TKey, TValue, compare, t>::BSPTreeNodeMiddle *, size_t>
BSP_tree<TKey, TValue, compare, t>::find_parent_recurse(BSPTreeNodeBase *root, BSPTreeNodeBase *child) {
    if (!root || root == child || root->_is_terminated) {
        return {nullptr, 0};
    }
    auto *mid = static_cast<BSPTreeNodeMiddle *>(root);
    for (size_t j = 0; j < mid->_pointers.size(); ++j) {
        if (mid->_pointers[j] == child) {
            return {mid, j};
        }
        if (auto sub = find_parent_recurse(mid->_pointers[j], child); sub.first) {
            return sub;
        }
    }
    return {nullptr, 0};
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
std::pair<typename BSP_tree<TKey, TValue, compare, t>::BSPTreeNodeMiddle *, size_t>
BSP_tree<TKey, TValue, compare, t>::find_parent_of(BSPTreeNodeBase *child) const {
    return find_parent_recurse(_root, child);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
size_t BSP_tree<TKey, TValue, compare, t>::node_size(BSPTreeNodeBase *n) const noexcept {
    if (!n) return 0;

    if (n->_is_terminated) {
        return static_cast<BSPTreeNodeTerm *>(n)->_data.size();
    }
    return static_cast<BSPTreeNodeMiddle *>(n)->_keys.size();
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
bool BSP_tree<TKey, TValue, compare, t>::borrow_leaf_right(BSPTreeNodeMiddle *parent, size_t i) {
    if (i + 1 >= parent->_pointers.size()) {
        return false;
    }
    auto *L = static_cast<BSPTreeNodeTerm *>(parent->_pointers[i]);
    auto *R = static_cast<BSPTreeNodeTerm *>(parent->_pointers[i + 1]);
    if (R->_data.size() <= minimum_keys_in_node) {
        return false;
    }
    L->_data.push_back(std::move(R->_data.front()));
    R->_data.erase(R->_data.begin());
    parent->_keys[i] = R->_data.front().first;
    return true;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
bool BSP_tree<TKey, TValue, compare, t>::borrow_leaf_left(BSPTreeNodeMiddle *parent, size_t i) {
    if (i == 0) {
        return false;
    }
    auto *L = static_cast<BSPTreeNodeTerm *>(parent->_pointers[i - 1]);
    auto *cur = static_cast<BSPTreeNodeTerm *>(parent->_pointers[i]);
    if (L->_data.size() <= minimum_keys_in_node) {
        return false;
    }
    cur->_data.insert(cur->_data.begin(), std::move(L->_data.back()));
    L->_data.pop_back();
    parent->_keys[i - 1] = cur->_data.front().first;
    return true;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
bool BSP_tree<TKey, TValue, compare, t>::borrow_internal_right(BSPTreeNodeMiddle *parent, size_t i) {
    if (i + 1 >= parent->_pointers.size()) {
        return false;
    }
    auto *z = static_cast<BSPTreeNodeMiddle *>(parent->_pointers[i]);
    auto *rs = static_cast<BSPTreeNodeMiddle *>(parent->_pointers[i + 1]);
    if (rs->_keys.size() <= minimum_keys_in_node) {
        return false;
    }
    z->_keys.push_back(std::move(parent->_keys[i]));
    parent->_keys[i] = std::move(rs->_keys.front());
    rs->_keys.erase(rs->_keys.begin());
    z->_pointers.push_back(rs->_pointers.front());
    rs->_pointers.erase(rs->_pointers.begin());
    return true;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
bool BSP_tree<TKey, TValue, compare, t>::borrow_internal_left(BSPTreeNodeMiddle *parent, size_t i) {
    if (i == 0) {
        return false;
    }
    auto *ls = static_cast<BSPTreeNodeMiddle *>(parent->_pointers[i - 1]);
    auto *z = static_cast<BSPTreeNodeMiddle *>(parent->_pointers[i]);
    if (ls->_keys.size() <= minimum_keys_in_node) {
        return false;
    }
    z->_keys.insert(z->_keys.begin(), std::move(parent->_keys[i - 1]));
    parent->_keys[i - 1] = std::move(ls->_keys.back());
    ls->_keys.pop_back();
    z->_pointers.insert(z->_pointers.begin(), ls->_pointers.back());
    ls->_pointers.pop_back();
    return true;
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::shrink_root_if_needed() {
    if (!_root || _root->_is_terminated) {
        return;
    }
    if (auto *m = static_cast<BSPTreeNodeMiddle *>(_root); m->_pointers.size() == 1) {
        _root = m->_pointers[0];
        _allocator.template delete_object<BSPTreeNodeMiddle>(m);
    }
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::erase_node(const BSPTreeNodeMiddle *node, const TKey &k) {
    (void) node;

    if (!_root) {
        return;
    }

    using leaf_t = BSPTreeNodeTerm;
    using middle_t = BSPTreeNodeMiddle;

    BSPTreeNodeBase *cur = _root;
    while (!cur->_is_terminated) {
        auto *mid = static_cast<middle_t *>(cur);
        const size_t i = key_index(mid, k);
        cur = mid->_pointers[i];
    }

    auto *leaf = static_cast<leaf_t *>(cur);
    const size_t idx = key_index(leaf, k, false);
    if (idx >= leaf->_data.size() || !keys_equal(leaf->_data[idx].first, k)) {
        return;
    }

    leaf->_data.erase(leaf->_data.begin() + idx);
    --_size;

    if (_size == 0) {
        _allocator.template delete_object<leaf_t>(leaf);
        _root = nullptr;
        return;
    }

    cur = leaf;

    while (true) {
        const size_t sz = node_size(cur);
        if (const bool underflow = cur != _root && sz < minimum_keys_in_node; !underflow) {
            shrink_root_if_needed();
            break;
        }

        auto pr = find_parent_of(cur);
        auto *parent = pr.first;
        const size_t ci = pr.second;
        if (!parent) {
            break;
        }

        if (cur->_is_terminated) {
            if (borrow_leaf_right(parent, ci) || borrow_leaf_left(parent, ci)) {
                break;
            }
            if (ci + 1 < parent->_pointers.size()) {
                merge_nodes(parent, ci);
            } else {
                merge_nodes(parent, ci - 1);
            }
        } else {
            if (borrow_internal_right(parent, ci) || borrow_internal_left(parent, ci)) {
                break;
            }
            if (ci + 1 < parent->_pointers.size()) {
                merge_nodes(parent, ci);
            } else {
                merge_nodes(parent, ci - 1);
            }
        }

        cur = parent;
    }

    shrink_root_if_needed();
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
BSP_tree<TKey, TValue, compare, t>::BSPTreeIterator
BSP_tree<TKey, TValue, compare, t>::bound(const TKey &key, bool strict) {
    if (!_root) return end();

    auto current = _root;

    while (!current->_is_terminated) {
        auto middle = static_cast<BSPTreeNodeMiddle *>(current);
        current = middle->_pointers[key_index(middle, key)];
    }

    auto leaf = static_cast<BSPTreeNodeTerm *>(current);
    const size_t index = key_index(leaf, key, strict);

    if (index < leaf->_data.size()) return BSPTreeIterator(leaf, index);
    if (leaf->_next) return BSPTreeIterator(leaf->_next, 0);
    return end();
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
bool BSP_tree<TKey, TValue, compare, t>::keys_equal(const TKey &a, const TKey &b) const noexcept {
    return !compare_keys(a, b) && !compare_keys(b, a);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::insert_into_middle(
    BSPTreeNodeMiddle *parent,
    size_t child_idx,
    TKey sep,
    BSPTreeNodeBase *right_child) {
    parent->_keys.insert(parent->_keys.begin() + child_idx, std::move(sep));
    parent->_pointers.insert(parent->_pointers.begin() + child_idx + 1, right_child);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::maybe_split_root() {
    if (!_root) return;

    if (!_root->_is_terminated) {
        if (const auto *mid = static_cast<BSPTreeNodeMiddle *>(_root); mid->_keys.size() <= maximum_keys_in_root) {
            return;
        }
    } else {
        if (const auto *leaf = static_cast<BSPTreeNodeTerm *>(_root); leaf->_data.size() <= maximum_keys_in_root) {
            return;
        }
    }

    auto *new_root = _allocator.template new_object<BSPTreeNodeMiddle>();
    new_root->_pointers.push_back(_root);
    _root = new_root;

    if (new_root->_pointers[0]->_is_terminated) {
        split_leaf_child(new_root, 0);
    } else {
        split_internal_child(new_root, 0);
    }
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::split_leaf_child(BSPTreeNodeMiddle *parent, size_t i) {
    auto *leaf = static_cast<BSPTreeNodeTerm *>(parent->_pointers[i]);
    auto *new_right = _allocator.template new_object<BSPTreeNodeTerm>();
    const size_t split_at = leaf->_data.size() / 2;
    for (size_t j = split_at; j < leaf->_data.size(); ++j) {
        new_right->_data.push_back(std::move(leaf->_data[j]));
    }
    leaf->_data.resize(split_at);
    new_right->_next = leaf->_next;
    leaf->_next = new_right;
    TKey sep = new_right->_data.front().first;
    insert_into_middle(parent, i, std::move(sep), new_right);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::split_internal_child(BSPTreeNodeMiddle *parent, size_t i) {
    auto *z = static_cast<BSPTreeNodeMiddle *>(parent->_pointers[i]);
    auto *y = _allocator.template new_object<BSPTreeNodeMiddle>();
    const size_t mid = z->_keys.size() / 2;
    TKey mid_key = std::move(z->_keys[mid]);
    for (size_t j = mid + 1; j < z->_keys.size(); ++j) {
        y->_keys.push_back(std::move(z->_keys[j]));
    }
    for (size_t j = mid + 1; j < z->_pointers.size(); ++j) {
        y->_pointers.push_back(z->_pointers[j]);
    }
    z->_keys.resize(mid);
    z->_pointers.resize(mid + 1);
    insert_into_middle(parent, i, std::move(mid_key), y);
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::insert_nonfull(BSPTreeNodeBase *x, tree_data_type &&data) {
    const TKey &k = data.first;

    if (x->_is_terminated) {
        auto *leaf = static_cast<BSPTreeNodeTerm *>(x);
        const size_t idx = key_index(leaf, k, false);
        leaf->_data.insert(leaf->_data.begin() + idx, std::move(data));
        return;
    }

    auto *mid = static_cast<BSPTreeNodeMiddle *>(x);
    size_t i = key_index(mid, k);
    BSPTreeNodeBase *ch = mid->_pointers[i];
    const bool full = ch->_is_terminated
                          ? static_cast<BSPTreeNodeTerm *>(ch)->_data.size() >= maximum_keys_in_node
                          : static_cast<BSPTreeNodeMiddle *>(ch)->_keys.size() >= maximum_keys_in_node;

    if (full) {
        if (ch->_is_terminated) {
            split_leaf_child(mid, i);
        } else {
            split_internal_child(mid, i);
        }
        i = key_index(mid, k);
    }

    insert_nonfull(mid->_pointers[i], std::move(data));
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare,
    t>::collect_leaves_inorder(BSPTreeNodeBase *n, std::vector<BSPTreeNodeTerm *> &out) {
    if (n->_is_terminated) {
        out.push_back(static_cast<BSPTreeNodeTerm *>(n));
        return;
    }
    auto *m = static_cast<BSPTreeNodeMiddle *>(n);
    for (size_t j = 0; j < m->_pointers.size(); ++j) {
        collect_leaves_inorder(m->_pointers[j], out);
    }
}

template<typename TKey, typename TValue, comparator<TKey> compare, std::size_t t>
void BSP_tree<TKey, TValue, compare, t>::relink_leaves() {
    if (!_root) return;

    std::vector<BSPTreeNodeTerm *> leaves;
    leaves.reserve(_size + 1);
    collect_leaves_inorder(_root, leaves);
    for (size_t i = 0; i + 1 < leaves.size(); ++i) {
        leaves[i]->_next = leaves[i + 1];
    }
    if (!leaves.empty()) {
        leaves.back()->_next = nullptr;
    }
}

// endregion BSP_tree helpers implementations
