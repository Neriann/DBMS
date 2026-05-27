#pragma once

#include "auth/session_context.hpp"
#include "rbac/permission.hpp"
#include "rbac/permission_resolver.hpp"

#include <string>

namespace rbac {

class AccessManager {
public:
    explicit AccessManager(PermissionResolver &resolver);

    [[nodiscard]] bool allow(const auth::SessionContext &ctx,
                             const std::string &database_name,
                             const std::string &table_name,
                             Permission permission) const;

private:
    PermissionResolver &resolver_;
};

} // namespace rbac
