#include "server/middleware/auth_middleware.hpp"
#include "support/in_memory_account_storage.hpp"
#include "auth/token_payload.hpp"

#include <chrono>
#include <gtest/gtest.h>

namespace {

crow::request make_request() {
    crow::request req;
    req.method = crow::HTTPMethod::Post;
    req.raw_url = "/query";
    req.url = "/query";
    return req;
}

} // namespace

TEST(AuthMiddlewareIntegration, RejectsMissingAuthorizationHeader) {
    tests::InMemoryAccountStorage storage;
    auth::JwtService jwt("alabuga");
    auth::PasswordHasher hasher;
    auth::AuthService auth_service(storage, jwt, hasher);
    server::AuthMiddleware middleware(auth_service);

    auto req = make_request();
    crow::response res;
    server::AuthMiddleware::context ctx;

    middleware.before_handle(req, res, ctx);

    EXPECT_EQ(res.code, crow::status::UNAUTHORIZED);
    EXPECT_FALSE(res.body.empty());
}

TEST(AuthMiddlewareIntegration, RejectsInvalidBearerToken) {
    tests::InMemoryAccountStorage storage;
    auth::JwtService jwt("alabuga");
    auth::PasswordHasher hasher;
    auth::AuthService auth_service(storage, jwt, hasher);
    server::AuthMiddleware middleware(auth_service);

    auto req = make_request();
    req.add_header("Authorization", "Token abc");
    crow::response res;
    server::AuthMiddleware::context ctx;

    middleware.before_handle(req, res, ctx);

    EXPECT_EQ(res.code, crow::status::UNAUTHORIZED);
}

TEST(AuthMiddlewareIntegration, AcceptsValidToken) {
    tests::InMemoryAccountStorage storage;
    auth::JwtService jwt("alabuga");
    auth::PasswordHasher hasher;
    auth::AuthService auth_service(storage, jwt, hasher);
    server::AuthMiddleware middleware(auth_service);

    auth::User user;
    user.id = "u1";
    user.username = "alice";
    user.salt = hasher.generate_salt();
    user.password_hash = hasher.hash_password("secret", user.salt);
    storage.save_user(user);

    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    auth::TokenPayload payload;
    payload.user_id = "u1";
    payload.username = "alice";
    payload.issued_at = static_cast<std::int64_t>(now);
    payload.expiration = static_cast<std::int64_t>(now + 3600);

    const auto token = jwt.issue_token(payload);

    auto req = make_request();
    req.add_header("Authorization", "Bearer " + token);
    crow::response res;
    server::AuthMiddleware::context ctx;

    middleware.before_handle(req, res, ctx);

    EXPECT_EQ(res.code, crow::status::OK);
    EXPECT_TRUE(ctx.session.is_authenticated);
    EXPECT_EQ(ctx.session.user_id, "u1");
}
