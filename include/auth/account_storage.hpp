#pragma once

#include "auth/user.hpp"

#include <optional>
#include <string>

namespace auth {

class AccountStorage {
public:
    virtual ~AccountStorage() = default;

    [[nodiscard]] virtual std::optional<User> find_by_username(const std::string &username) const = 0;
    [[nodiscard]] virtual std::optional<User> find_by_id(const std::string &user_id) const = 0;
    virtual void save_user(const User &user) = 0;
};

} // namespace auth

