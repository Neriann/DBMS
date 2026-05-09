#include "core/value.hpp"

bool ValueComparator::operator()(const Value &a, const Value &b) const {
    if (a.index() != b.index()) {
        return a.index() < b.index();
    }
    if (std::holds_alternative<int>(a)) {
        return std::get<int>(a) < std::get<int>(b);
    }
    if (std::holds_alternative<std::string>(a)) {
        return std::get<std::string>(a) < std::get<std::string>(b);
    }

    return false;
}
