#include "auth/auth_service.hpp"

#include <chrono>
#include <random>
#include <stdexcept>

namespace auth {
namespace {

std::string generate_user_id() {
    std::random_device device;
    std::mt19937_64 engine(device());
    std::uniform_int_distribution<std::uint64_t> dist;
    const auto value = dist(engine);
    return std::to_string(value);
}

std::int64_t now_seconds() {
    const auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
}

} // namespace

AuthService::AuthService(AccountStorage &storage, JwtService &jwt, PasswordHasher &hasher)
    : storage_(storage), jwt_(jwt), hasher_(hasher) {
}

std::string AuthService::login(const std::string &username, const std::string &password) {
    const auto user = storage_.find_by_username(username);
    if (!user.has_value()) {
        throw std::runtime_error("user not found");
    }
    if (!hasher_.verify_password(password, user->salt, user->password_hash)) {
        throw std::runtime_error("invalid credentials");
    }

    TokenPayload payload;
    payload.user_id = user->id;
    payload.username = user->username;
    payload.issued_at = now_seconds();
    payload.expiration = payload.issued_at + 3600; // 1 hour expiration

    return jwt_.issue_token(payload);
}

std::string AuthService::register_user(const std::string &username, const std::string &password) {
    if (storage_.find_by_username(username).has_value()) {
        throw std::runtime_error("user already exists");
    }

    const bool first_user = !storage_.has_users();

    User user;
    user.id = generate_user_id();
    user.username = username;
    user.salt = hasher_.generate_salt();
    user.password_hash = hasher_.hash_password(password, user.salt);
    user.is_admin = first_user;

    storage_.save_user(user);

    TokenPayload payload;
    payload.user_id = user.id;
    payload.username = user.username;
    payload.issued_at = now_seconds();
    payload.expiration = payload.issued_at + 3600;

    return jwt_.issue_token(payload);
}

SessionContext AuthService::authenticate(const std::string &token) const {
    if (!jwt_.validate(token)) {
        throw std::runtime_error("invalid token");
    }

    const auto payload = jwt_.parse(token);
    const auto now = now_seconds();
    if (payload.expiration < now) {
        throw std::runtime_error("token expired");
    }

    const auto user = storage_.find_by_id(payload.user_id);
    if (!user.has_value()) {
        throw std::runtime_error("user not found");
    }
    if (!payload.username.empty() && user->username != payload.username) {
        throw std::runtime_error("token user mismatch");
    }

    SessionContext ctx;
    ctx.user_id = user->id;
    ctx.username = user->username;
    ctx.is_admin = user->is_admin;
    ctx.is_authenticated = true;
    return ctx;
}

} // namespace auth
