#pragma once

#include <string>

namespace auth {

struct SessionContext {
    std::string user_id;
    std::string username;
    bool is_authenticated = false;
    bool is_admin = false;
};

} // namespace auth

