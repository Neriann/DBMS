#include "core/string_interner.hpp"

const std::string* StringInterner::intern(const std::string& str) {
    auto [it, inserted] = pool_.insert(str);
    return &(*it);
}

StringInterner& StringInterner::instance() {
    static StringInterner instance;
    return instance;
}