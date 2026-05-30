#pragma once

#include "auth/account_storage.hpp"
#include "auth/jwt_service.hpp"
#include "auth/password_hasher.hpp"
#include "auth/session_context.hpp"

#include <string>

namespace auth {

class AuthService {
public:
    AuthService(AccountStorage &storage, JwtService &jwt, PasswordHasher &hasher);

    [[nodiscard]] std::string login(const std::string &username, const std::string &password);
    [[nodiscard]] std::string register_user(const std::string &username, const std::string &password);
    [[nodiscard]] SessionContext authenticate(const std::string &token) const;

private:
    AccountStorage &storage_;
    JwtService &jwt_;
    PasswordHasher &hasher_;
};

} // namespace auth

