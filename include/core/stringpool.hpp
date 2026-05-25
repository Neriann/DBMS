#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstddef>

using StringId = std::size_t;

struct InternedString {
    StringId id;
};

class StringPool {
public:
    StringId intern(const std::string& str);

    const std::string& get(StringId id) const;

    bool contains(const std::string& str) const;

    std::size_t size() const noexcept;

private:
    std::vector<std::string> strings_;
    std::unordered_map<std::string, StringId> ids_;
};