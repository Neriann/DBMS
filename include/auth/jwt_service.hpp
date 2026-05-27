#pragma once

#include "auth/token_payload.hpp"

#include <string>

namespace auth {

class JwtService {
public:
    [[nodiscard]] std::string issue_token(const TokenPayload &payload) const;
    [[nodiscard]] bool validate(const std::string &token) const;
    [[nodiscard]] TokenPayload parse(const std::string &token) const;
};

} // namespace auth

