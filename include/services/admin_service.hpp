#pragma once

#include "rbac/access_control_storage.hpp"
#include "rbac/permission.hpp"
#include "rbac/policy.hpp"

#include <string>

namespace services {

class AdminService {
public:
    explicit AdminService(rbac::AccessControlStorage &storage);

    [[nodiscard]] std::string create_group(const std::string &name);
    [[nodiscard]] std::string add_user_to_group(const std::string &user_id, const std::string &group_id);
    [[nodiscard]] std::string grant_permission(rbac::SubjectType subject_type,
                                               const std::string &subject_id,
                                               const std::string &database_name,
                                               const std::string &table_name,
                                               rbac::Permission permission);
    [[nodiscard]] std::string revoke_permission(rbac::SubjectType subject_type,
                                                const std::string &subject_id,
                                                const std::string &database_name,
                                                const std::string &table_name,
                                                rbac::Permission permission);

private:
    rbac::AccessControlStorage &storage_;
};

} // namespace services
