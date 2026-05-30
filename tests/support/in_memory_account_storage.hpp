#pragma once

#include "auth/account_storage.hpp"

#include <optional>
#include <string>
#include <unordered_map>

namespace tests {

class InMemoryAccountStorage : public auth::AccountStorage {
public:
    [[nodiscard]] std::optional<auth::User> find_by_username(const std::string &username) const override {
        auto it = by_username_.find(username);
        if (it == by_username_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    [[nodiscard]] std::optional<auth::User> find_by_id(const std::string &user_id) const override {
        auto it = by_id_.find(user_id);
        if (it == by_id_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    [[nodiscard]] bool has_users() const override {
        return !by_id_.empty();
    }

    void save_user(const auth::User &user) override {
        by_username_[user.username] = user;
        by_id_[user.id] = user;
    }

    [[nodiscard]] std::size_t size() const {
        return by_id_.size();
    }

private:
    std::unordered_map<std::string, auth::User> by_username_;
    std::unordered_map<std::string, auth::User> by_id_;
};

} // namespace tests
