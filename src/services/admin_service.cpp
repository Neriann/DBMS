#include "services/admin_service.hpp"

#include "common/not_implemented.hpp"

namespace services {

std::string AdminService::create_user(const std::string &username, const std::string &password) {
    common::not_implemented(username, password);
    return {};
}

std::string AdminService::grant_permission(const std::string &user_id,
                                           const std::string &database_name,
                                           const std::string &table_name,
                                           const rbac::Permission permission) {
    common::not_implemented(user_id, database_name, table_name, permission);
    return {};
}

} // namespace services

