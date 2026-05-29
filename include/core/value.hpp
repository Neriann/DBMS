#pragma once
#include <variant>
#include <string>
#include <cstddef>
#include "stringpool.hpp"

using Value = std::variant<
    int,
    InternedString,
    std::nullptr_t
>;

class ValueComparator {
public:
    ValueComparator() = default;

    explicit ValueComparator(const StringPool* pool)
        : pool_(pool) {}

    ValueComparator(const ValueComparator&) = default;
    ValueComparator(ValueComparator&&) = default;

    ValueComparator& operator=(const ValueComparator&) = default;
    ValueComparator& operator=(ValueComparator&&) = default;

    bool operator()(const Value& lhs, const Value& rhs) const;

private:
    const StringPool* pool_ = nullptr;
};