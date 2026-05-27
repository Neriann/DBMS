#include "rbac/access_manager.hpp"

#include "common/not_implemented.hpp"

namespace rbac {

AccessManager::AccessManager(PermissionResolver &resolver)
    : resolver_(resolver) {
}

bool AccessManager::allow(const auth::SessionContext &ctx,
                          const std::string &database_name,
                          const std::string &table_name,
                          const Permission permission) const {
    common::not_implemented(ctx, database_name, table_name, permission);
    return false;
}

} // namespace rbac

