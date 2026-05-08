#pragma once
#include <variant>
#include <string>
#include <cstddef>

using Value = std::variant<int, std::string, std::nullptr_t>;

struct ValueComparator {
    bool operator()(const Value &a, const Value &b) const;
};
