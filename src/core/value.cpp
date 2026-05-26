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

        if (!pool_) {
            return l.id < r.id;
        }

        return pool_->get(l.id) < pool_->get(r.id);
    }

    return false;
}