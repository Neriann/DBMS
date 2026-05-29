#include "auth/jwt_service.hpp"

#include <jwt-cpp/jwt.h>

#include <chrono>
#include <stdexcept>

namespace auth {
    namespace {
        std::chrono::system_clock::time_point from_unix_seconds(std::int64_t seconds) {
            return std::chrono::system_clock::time_point{std::chrono::seconds(seconds)};
        }

        std::int64_t now_seconds() {
            const auto now = std::chrono::system_clock::now();
            return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        }
    } // namespace


    JwtService::JwtService(std::string secret) : secret_(std::move(secret)) {
    }

    std::string JwtService::issue_token(const TokenPayload &payload) const {
        return jwt::create()
                .set_type("JWT")
                .set_issuer("dbms")
                .set_subject(payload.user_id)
                .set_payload_claim("username", jwt::claim(payload.username))
                .set_issued_at(from_unix_seconds(payload.issued_at))
                .set_expires_at(from_unix_seconds(payload.expiration))
                .sign(jwt::algorithm::hs256{secret_});
    }

    bool JwtService::validate(const std::string &token) const {
        try {
            const auto decoded = jwt::decode(token);
            jwt::verify()
                    .allow_algorithm(jwt::algorithm::hs256{secret_})
                    .with_issuer("dbms")
                    .verify(decoded);

            const auto exp = decoded.get_expires_at();
            const auto now = std::chrono::system_clock::now();
            return exp > now;
        } catch (...) {
            return false;
        }
    }

    TokenPayload JwtService::parse(const std::string &token) const {
        try {
            const auto decoded = jwt::decode(token);
            jwt::verify()
                    .allow_algorithm(jwt::algorithm::hs256{secret_})
                    .with_issuer("dbms")
                    .verify(decoded);

            const auto username_claim = decoded.get_payload_claim("username");
            if (username_claim.get_type() != jwt::json::type::string) {
                throw std::runtime_error("invalid username claim");
            }

            TokenPayload payload;
            payload.user_id = decoded.get_subject();
            payload.username = username_claim.as_string();
            payload.issued_at = std::chrono::duration_cast<std::chrono::seconds>(
                        decoded.get_issued_at().time_since_epoch())
                    .count();
            payload.expiration = std::chrono::duration_cast<std::chrono::seconds>(
                        decoded.get_expires_at().time_since_epoch())
                    .count();

            const auto now = now_seconds();
            if (payload.expiration < now) {
                throw std::runtime_error("token expired");
            }

            return payload;
        } catch (const std::exception &e) {
            throw std::runtime_error(std::string("invalid token: ") + e.what());
        }
    }
} // namespace auth
