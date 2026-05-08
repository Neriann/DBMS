#include "core/schema.hpp"
#include "not_implemented.h"

int find(const Schema &/*schema*/, const std::string &/*name*/) {
    throw not_implemented("int find(const Schema &, const std::string &)", "is not implemented");
}

bool Column::is_not_null() const noexcept {
    return constraints & NOT_NULL;
}

bool Column::is_indexed() const noexcept {
    return constraints & INDEXED;
}
