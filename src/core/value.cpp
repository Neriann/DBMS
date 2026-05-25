#include "core/value.hpp"
#include "core/stringpool.hpp"
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

        const std::string& ls = pool_.get(l.id);
        const std::string& rs = pool_.get(r.id);

        return ls < rs;
    }

    if (std::holds_alternative<std::nullptr_t>(lhs)) {
        return false; // NULLs equal or last
    }

    return false;
}