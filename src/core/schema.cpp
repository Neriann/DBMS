#include "core/schema.hpp"

int find(const Schema &schema, const std::string &name) {
    for (auto i = 0; i < schema.size(); ++i) {
        if (schema[i].name == name) return i;
    }

    return -1;
}

bool Column::is_not_null() const noexcept {
    return constraints & NOT_NULL;
}

bool Column::is_indexed() const noexcept {
    return constraints & INDEXED;
}
