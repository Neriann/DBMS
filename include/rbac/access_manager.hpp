#pragma once

#include "auth/session_context.hpp"
#include "rbac/access_control_storage.hpp"
#include "rbac/permission.hpp"
#include "rbac/permission_resolver.hpp"

#include <string>

namespace rbac {

class AccessManager {
public:
    AccessManager(PermissionResolver &resolver, AccessControlStorage &storage);

    [[nodiscard]] bool allow(const auth::SessionContext &ctx,
                             const std::string &database_name,
                             const std::string &table_name,
                             Permission permission) const;

private:
    PermissionResolver &resolver_;
    AccessControlStorage &storage_;
};

} // namespace rbac
