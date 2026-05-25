#include "core/stringpool.hpp"

StringId StringPool::intern(const std::string& str) {
    auto it = ids_.find(str);

    if (it != ids_.end()) {
        return it->second;
    }

    StringId id = strings_.size();

    strings_.push_back(str);
    ids_[str] = id;

    return id;
}

const std::string& StringPool::get(StringId id) const {
    return strings_.at(id);
}

bool StringPool::contains(const std::string& str) const {
    return ids_.contains(str);
}

std::size_t StringPool::size() const noexcept {
    return strings_.size();
}