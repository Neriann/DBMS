#include "auth/auth_service.hpp"

#include <random>

#include "common/not_implemented.hpp"

namespace auth {

AuthService::AuthService(AccountStorage &storage, JwtService &jwt, PasswordHasher &hasher)
    : storage_(storage), jwt_(jwt), hasher_(hasher) {
}

std::string AuthService::login(const std::string &username, const std::string &password) {
    common::not_implemented();
}

std::string AuthService::register_user(const std::string &username, const std::string &password) {
    common::not_implemented();
}

SessionContext AuthService::authenticate(const std::string &token) const {
    common::not_implemented();
}

} // namespace auth
