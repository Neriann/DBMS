#include "rbac/permission_resolver.hpp"
#include "auth/session_context.hpp"

#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace {

rbac::Policy make_policy(rbac::SubjectType type,
                         const std::string &subject_id,
                         const std::string &db,
                         const std::string &table,
                         rbac::Permission permission,
                         bool allowed) {
    rbac::Policy policy;
    policy.subject_type = type;
    policy.subject_id = subject_id;
    policy.database_name = db;
    policy.table_name = table;
    policy.permission = permission;
    policy.allowed = allowed;
    return policy;
}

} // namespace

TEST(PermissionResolverUnit, DefaultAllowAppliesWhenNoOverrides) {
    rbac::PermissionResolver resolver;

    auth::SessionContext ctx;
    ctx.user_id = "u1";
    ctx.username = "alice";

    const std::vector<rbac::Policy> policies = {
        make_policy(rbac::SubjectType::Default, "", "db1", "users", rbac::Permission::ReadTable, true)};

    const auto allowed = resolver.resolve(ctx, policies, {}, "db1", "users", rbac::Permission::ReadTable);
    EXPECT_TRUE(allowed);
}

TEST(PermissionResolverUnit, UserDenyOverridesGroupAllow) {
    rbac::PermissionResolver resolver;

    auth::SessionContext ctx;
    ctx.user_id = "u1";
    ctx.username = "alice";

    const std::vector<rbac::Policy> policies = {
        make_policy(rbac::SubjectType::Group, "g1", "db1", "users", rbac::Permission::ReadTable, true),
        make_policy(rbac::SubjectType::User, "u1", "db1", "users", rbac::Permission::ReadTable, false)};

    const auto allowed = resolver.resolve(ctx, policies, {"g1"}, "db1", "users", rbac::Permission::ReadTable);
    EXPECT_FALSE(allowed);
}

TEST(PermissionResolverUnit, UserAllowOverridesGroupDeny) {
    rbac::PermissionResolver resolver;

    auth::SessionContext ctx;
    ctx.user_id = "u1";
    ctx.username = "alice";

    const std::vector<rbac::Policy> policies = {
        make_policy(rbac::SubjectType::Group, "g1", "db1", "users", rbac::Permission::ReadTable, false),
        make_policy(rbac::SubjectType::User, "u1", "db1", "users", rbac::Permission::ReadTable, true)};

    const auto allowed = resolver.resolve(ctx, policies, {"g1"}, "db1", "users", rbac::Permission::ReadTable);
    EXPECT_TRUE(allowed);
}

TEST(PermissionResolverUnit, GroupAllowOverridesDefaultDeny) {
    rbac::PermissionResolver resolver;

    auth::SessionContext ctx;
    ctx.user_id = "u1";
    ctx.username = "alice";

    const std::vector<rbac::Policy> policies = {
        make_policy(rbac::SubjectType::Default, "", "db1", "users", rbac::Permission::ReadTable, false),
        make_policy(rbac::SubjectType::Group, "g1", "db1", "users", rbac::Permission::ReadTable, true)};

    const auto allowed = resolver.resolve(ctx, policies, {"g1"}, "db1", "users", rbac::Permission::ReadTable);
    EXPECT_TRUE(allowed);
}

TEST(PermissionResolverUnit, PolicyDoesNotApplyToOtherTables) {
    rbac::PermissionResolver resolver;

    auth::SessionContext ctx;
    ctx.user_id = "u1";
    ctx.username = "alice";

    const std::vector<rbac::Policy> policies = {
        make_policy(rbac::SubjectType::Default, "", "db1", "users", rbac::Permission::ReadTable, true)};

    const auto allowed = resolver.resolve(ctx, policies, {}, "db1", "orders", rbac::Permission::ReadTable);
    EXPECT_FALSE(allowed);
}

TEST(PermissionResolverUnit, WildcardDefaultReadAppliesToAnyTable) {
    rbac::PermissionResolver resolver;

    auth::SessionContext ctx;
    ctx.user_id = "u1";
    ctx.username = "alice";

    const std::vector<rbac::Policy> policies = {
        make_policy(rbac::SubjectType::Default, "", "*", "*", rbac::Permission::ReadTable, true)};

    EXPECT_TRUE(resolver.resolve(ctx, policies, {}, "db1", "users", rbac::Permission::ReadTable));
    EXPECT_TRUE(resolver.resolve(ctx, policies, {}, "db2", "reports", rbac::Permission::ReadTable));
    EXPECT_FALSE(resolver.resolve(ctx, policies, {}, "db1", "users", rbac::Permission::WriteTable));
}

TEST(PermissionResolverUnit, UserSpecificDenyOverridesWildcardDefaultRead) {
    rbac::PermissionResolver resolver;

    auth::SessionContext ctx;
    ctx.user_id = "u1";
    ctx.username = "alice";

    const std::vector<rbac::Policy> policies = {
        make_policy(rbac::SubjectType::Default, "", "*", "*", rbac::Permission::ReadTable, true),
        make_policy(rbac::SubjectType::User, "u1", "db1", "users", rbac::Permission::ReadTable, false)};

    EXPECT_FALSE(resolver.resolve(ctx, policies, {}, "db1", "users", rbac::Permission::ReadTable));
    EXPECT_TRUE(resolver.resolve(ctx, policies, {}, "db1", "orders", rbac::Permission::ReadTable));
}

TEST(PermissionResolverUnit, AdminBypassesChecks) {
    rbac::PermissionResolver resolver;

    auth::SessionContext ctx;
    ctx.user_id = "admin";
    ctx.username = "root";
    ctx.is_admin = true;

    const std::vector<rbac::Policy> policies = {
        make_policy(rbac::SubjectType::Default, "", "db1", "users", rbac::Permission::ReadTable, false)};

    const auto allowed = resolver.resolve(ctx, policies, {}, "db1", "users", rbac::Permission::ReadTable);
    EXPECT_TRUE(allowed);
}
