#pragma once

#include <string>

namespace auth {

struct User {
    std::string id;
    std::string username;
    std::string password_hash;
    std::string salt;
    bool is_admin = false;
};

} // namespace auth

