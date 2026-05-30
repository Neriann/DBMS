#pragma once

#include "auth/session_context.hpp"
#include "rbac/policy.hpp"

#include <string>
#include <vector>

namespace rbac {

class PermissionResolver {
public:
    [[nodiscard]] bool resolve(const auth::SessionContext &ctx,
                               const std::vector<Policy> &policies,
                               const std::vector<std::string> &group_ids,
                               const std::string &database_name,
                               const std::string &table_name,
                               Permission permission) const;
};

} // namespace rbac

