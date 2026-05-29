#pragma once

#include "core/stringpool.hpp"
#include "core/value.hpp"

#include <string>

inline Value interned_value(const std::string &value) {
    return InternedString{global_string_pool().intern(value)};
}

inline const std::string &interned_string(const Value &value) {
    return global_string_pool().get(std::get<InternedString>(value).id);
}
