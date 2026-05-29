#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <string_view>
#include <optional>
#include <cstddef>

using StringId = std::size_t;

struct InternedString {
    StringId id;

    bool operator==(const InternedString& other) const {
        return id == other.id;
    }
    bool operator!=(const InternedString& other) const {
        return id != other.id;
    }
    bool operator<(const InternedString& other) const {
        return id < other.id;
    }
    bool operator>(const InternedString& other) const {
        return id > other.id;
    }
};

class StringPool {
public:
    StringId intern(const std::string& str);

    const std::string& get(StringId id) const;

    bool contains(const std::string& str) const;

    std::size_t size() const noexcept;

private:
    struct Entry {
        std::string value;
        std::size_t refcount = 0;
    };

    std::vector<std::optional<Entry>> slots_;
    std::vector<StringId> free_list_;
    std::unordered_map<std::string_view, StringId> ids_;
};

inline StringPool& global_string_pool() {
    static StringPool pool;
    return pool;
}