#pragma once

#include <string>
#include <unordered_set>

class StringInterner {
public:
    /**
     * Returns pointer to canonical string instance.
     * Equal strings share the same memory.
     */
    const std::string* intern(const std::string& str);

    /**
     * Global singleton instance.
     */
    static StringInterner& instance();

    StringInterner(const StringInterner&) = delete;
    StringInterner& operator=(const StringInterner&) = delete;

private:
    StringInterner() = default;

    std::unordered_set<std::string> pool_;
};