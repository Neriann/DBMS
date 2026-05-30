#include "rbac/access_manager.hpp"

namespace rbac {

AccessManager::AccessManager(PermissionResolver &resolver, AccessControlStorage &storage)
    : resolver_(resolver), storage_(storage) {
}

bool AccessManager::allow(const auth::SessionContext &ctx,
                          const std::string &database_name,
                          const std::string &table_name,
                          const Permission permission) const {
    if (!ctx.is_authenticated) {
        return false;
    }
    return resolver_.resolve(ctx,
                             storage_.policies(),
                             storage_.group_ids_for_user(ctx.user_id),
                             database_name,
                             table_name,
                             permission);
}

} // namespace rbac
