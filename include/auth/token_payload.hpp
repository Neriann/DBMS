#pragma once

#include <cstdint>
#include <string>

namespace auth {

struct TokenPayload {
    std::string user_id;
    std::string username;
    std::int64_t issued_at = 0;
    std::int64_t expiration = 0;
};

} // namespace auth

