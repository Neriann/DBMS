#include "auth/auth_service.hpp"
#include "auth/jwt_service.hpp"
#include "auth/password_hasher.hpp"
#include "auth/token_payload.hpp"
#include "support/in_memory_account_storage.hpp"

#include <chrono>
#include <gtest/gtest.h>
#include <stdexcept>

namespace {

auth::User make_user(auth::PasswordHasher &hasher,
                     const std::string &id,
                     const std::string &username,
                     const std::string &password,
                     bool is_admin = false) {
    auth::User user;
    user.id = id;
    user.username = username;
    user.salt = hasher.generate_salt();
    user.password_hash = hasher.hash_password(password, user.salt);
    user.is_admin = is_admin;
    return user;
}

} // namespace

TEST(AuthServiceIntegration, RegisterStoresUserAndReturnsToken) {
    tests::InMemoryAccountStorage storage;
    auth::JwtService jwt("alabuga");
    auth::PasswordHasher hasher;
    auth::AuthService auth_service(storage, jwt, hasher);

    const auto token = auth_service.register_user("alice", "secret");

    EXPECT_FALSE(token.empty());
    EXPECT_EQ(storage.size(), 1u);
    const auto user = storage.find_by_username("alice");
    ASSERT_TRUE(user.has_value());
    EXPECT_FALSE(user->salt.empty());
    EXPECT_FALSE(user->password_hash.empty());
    EXPECT_NE(user->password_hash, "secret");
}

TEST(AuthServiceIntegration, RegisterRejectsDuplicateUser) {
    tests::InMemoryAccountStorage storage;
    auth::JwtService jwt("alabuga");
    auth::PasswordHasher hasher;
    auth::AuthService auth_service(storage, jwt, hasher);

    const auto token = auth_service.register_user("alice", "secret");
    EXPECT_FALSE(token.empty());

    EXPECT_THROW({
        const auto duplicate = auth_service.register_user("alice", "secret");
        (void)duplicate;
    }, std::runtime_error);
}

TEST(AuthServiceIntegration, LoginSuccessReturnsToken) {
    tests::InMemoryAccountStorage storage;
    auth::JwtService jwt("alabuga");
    auth::PasswordHasher hasher;
    auth::AuthService auth_service(storage, jwt, hasher);

    storage.save_user(make_user(hasher, "u1", "alice", "secret"));

    const auto token = auth_service.login("alice", "secret");
    EXPECT_FALSE(token.empty());
}

TEST(AuthServiceIntegration, LoginRejectsUnknownUser) {
    tests::InMemoryAccountStorage storage;
    auth::JwtService jwt("alabuga");
    auth::PasswordHasher hasher;
    auth::AuthService auth_service(storage, jwt, hasher);

    EXPECT_THROW({
        const auto token = auth_service.login("alice", "secret");
        (void)token;
    }, std::runtime_error);
}

TEST(AuthServiceIntegration, LoginRejectsBadPassword) {
    tests::InMemoryAccountStorage storage;
    auth::JwtService jwt("alabuga");
    auth::PasswordHasher hasher;
    auth::AuthService auth_service(storage, jwt, hasher);

    storage.save_user(make_user(hasher, "u1", "alice", "secret"));

    EXPECT_THROW({
        const auto token = auth_service.login("alice", "wrong");
        (void)token;
    }, std::runtime_error);
}

TEST(AuthServiceIntegration, AuthenticateValidTokenReturnsSessionContext) {
    tests::InMemoryAccountStorage storage;
    auth::JwtService jwt("alabuga");
    auth::PasswordHasher hasher;
    auth::AuthService auth_service(storage, jwt, hasher);

    storage.save_user(make_user(hasher, "u1", "alice", "secret", true));

    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    auth::TokenPayload payload;
    payload.user_id = "u1";
    payload.username = "alice";
    payload.issued_at = static_cast<std::int64_t>(now);
    payload.expiration = static_cast<std::int64_t>(now + 3600);

    const auto token = jwt.issue_token(payload);
    const auto session = auth_service.authenticate(token);

    EXPECT_TRUE(session.is_authenticated);
    EXPECT_EQ(session.user_id, "u1");
    EXPECT_EQ(session.username, "alice");
    EXPECT_TRUE(session.is_admin);
}

TEST(AuthServiceIntegration, AuthenticateRejectsExpiredToken) {
    tests::InMemoryAccountStorage storage;
    auth::JwtService jwt("alabuga");
    auth::PasswordHasher hasher;
    auth::AuthService auth_service(storage, jwt, hasher);

    storage.save_user(make_user(hasher, "u1", "alice", "secret"));

    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    auth::TokenPayload payload;
    payload.user_id = "u1";
    payload.username = "alice";
    payload.issued_at = static_cast<std::int64_t>(now - 10);
    payload.expiration = static_cast<std::int64_t>(now - 1);

    const auto token = jwt.issue_token(payload);

    EXPECT_THROW({
        const auto session = auth_service.authenticate(token);
        (void)session;
    }, std::runtime_error);
}

TEST(JwtServiceIntegration, IssueValidateAndParseRoundTrip) {
    auth::JwtService jwt("alabuga");

    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    auth::TokenPayload payload;
    payload.user_id = "u-42";
    payload.username = "bob";
    payload.issued_at = static_cast<std::int64_t>(now);
    payload.expiration = static_cast<std::int64_t>(now + 120);

    const auto token = jwt.issue_token(payload);

    EXPECT_FALSE(token.empty());
    EXPECT_TRUE(jwt.validate(token));

    const auto parsed = jwt.parse(token);
    EXPECT_EQ(parsed.user_id, payload.user_id);
    EXPECT_EQ(parsed.username, payload.username);
    EXPECT_EQ(parsed.issued_at, payload.issued_at);
    EXPECT_EQ(parsed.expiration, payload.expiration);
}

TEST(JwtServiceIntegration, RejectsMalformedToken) {
    auth::JwtService jwt("alabuga");

    EXPECT_FALSE(jwt.validate("this.is.not.jwt"));
    EXPECT_THROW({
        const auto parsed = jwt.parse("this.is.not.jwt");
        (void)parsed;
    }, std::runtime_error);
}
