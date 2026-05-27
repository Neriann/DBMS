#include "rbac/permission_resolver.hpp"

#include "common/not_implemented.hpp"

namespace rbac {

bool PermissionResolver::resolve(const auth::SessionContext &ctx,
                                 const std::vector<Policy> &policies,
                                 const std::vector<std::string> &group_ids,
                                 const std::string &database_name,
                                 const std::string &table_name,
                                 const Permission permission) const {
    common::not_implemented(ctx, policies, group_ids, database_name, table_name, permission);
    return false;
}

} // namespace rbac

