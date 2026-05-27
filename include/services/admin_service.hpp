#pragma once

#include "auth/session_context.hpp"
#include "rbac/permission.hpp"

#include <string>

namespace services {

class AdminService {
public:
    [[nodiscard]] std::string create_user(const std::string &username, const std::string &password);
    [[nodiscard]] std::string grant_permission(const std::string &user_id,
                                              const std::string &database_name,
                                              const std::string &table_name,
                                              rbac::Permission permission);
};

} // namespace services

