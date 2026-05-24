#pragma once
#include <variant>
#include <string>
#include <cstddef>

using InternedString = const std::string*;

using Value = std::variant<
    int,
    InternedString,
    std::nullptr_t
>;

struct ValueComparator {
    bool operator()(const Value &a, const Value &b) const;
};
