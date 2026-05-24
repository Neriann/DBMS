#include "core/value.hpp"
#include "core/string_interner.hpp"
#include <variant>

bool ValueComparator::operator()(const Value& lhs, const Value& rhs) const {
    if (lhs.index() != rhs.index()) {
        return lhs.index() < rhs.index();
    }

    if (std::holds_alternative<int>(lhs)) {
        return std::get<int>(lhs) < std::get<int>(rhs);
    }

    if (std::holds_alternative<InternedString>(lhs)) {
        const auto& l = std::get<InternedString>(lhs);
        const auto& r = std::get<InternedString>(rhs);

        if (!l || !r) return l < r;

        return *l < *r;
    }

    return false;
}