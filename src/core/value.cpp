#include "core/value.hpp"
#include "not_implemented.h"

bool ValueComparator::operator()(const Value &/*a*/, const Value &/*b*/) const {
    throw not_implemented("bool ValueComparator::operator()(const Value &, const Value &) const", "is not implemented");
}
