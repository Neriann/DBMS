#include "core/stringpool.hpp"

StringId StringPool::intern(const std::string& str) {
    auto it = ids_.find(std::string_view(str));

    if (it != ids_.end()) {
        return it->second;
    }

    StringId id;

    if (!free_list_.empty()) {
        id = free_list_.back();
        free_list_.pop_back();
        slots_[id] = Entry{str, 1};
    } else {
        id = slots_.size();
        slots_.push_back(Entry{str, 1});
    }

    const std::string& stored = slots_[id]->value;
    std::string_view key(stored);

    ids_.emplace(key, id);

    return id;
}

const std::string& StringPool::get(StringId id) const {
    return slots_.at(id)->value;
}

bool StringPool::contains(const std::string& str) const {
    return ids_.find(str) != ids_.end();
}

std::size_t StringPool::size() const noexcept {
    return slots_.size() - free_list_.size();
}