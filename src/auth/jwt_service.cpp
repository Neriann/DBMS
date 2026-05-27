#include "auth/jwt_service.hpp"

#include <nlohmann/json.hpp>

#include "common/not_implemented.hpp"

namespace auth {

std::string JwtService::issue_token(const TokenPayload &payload) const {
    common::not_implemented();

}

bool JwtService::validate(const std::string &token) const {
    common::not_implemented();

}

TokenPayload JwtService::parse(const std::string &token) const {
    common::not_implemented();

}

} // namespace auth
