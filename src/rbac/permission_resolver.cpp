#include "rbac/permission_resolver.hpp"

#include <algorithm>

namespace rbac {
namespace {

bool matches_scope(const Policy &policy,
                   const std::string &database_name,
                   const std::string &table_name,
                   Permission permission) {
    return policy.permission == permission
        && policy.database_name == database_name
        && policy.table_name == table_name;
}

} // namespace

bool PermissionResolver::resolve(const auth::SessionContext &ctx,
                                 const std::vector<Policy> &policies,
                                 const std::vector<std::string> &group_ids,
                                 const std::string &database_name,
                                 const std::string &table_name,
                                 const Permission permission) const {
    if (ctx.is_admin) {
        return true;
    }

    bool has_user = false;
    bool user_allowed = false;
    bool has_group = false;
    bool group_allowed = false;
    bool has_default = false;
    bool default_allowed = false;

    for (const auto &policy : policies) {
        if (!matches_scope(policy, database_name, table_name, permission)) {
            continue;
        }

        switch (policy.subject_type) {
        case SubjectType::User:
            if (policy.subject_id == ctx.user_id) {
                has_user = true;
                user_allowed = policy.allowed;
            }
            break;
        case SubjectType::Group:
            if (std::find(group_ids.begin(), group_ids.end(), policy.subject_id) != group_ids.end()) {
                has_group = true;
                group_allowed = policy.allowed;
            }
            break;
        case SubjectType::Default:
            has_default = true;
            default_allowed = policy.allowed;
            break;
        }
    }

    if (has_user) {
        return user_allowed;
    }
    if (has_group) {
        return group_allowed;
    }
    if (has_default) {
        return default_allowed;
    }
    return false;
}

} // namespace rbac
