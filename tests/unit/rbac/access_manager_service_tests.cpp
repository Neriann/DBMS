#include "rbac/access_manager.hpp"

#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace {

class InMemoryAccessControlStorage final : public rbac::AccessControlStorage {
public:
    [[nodiscard]] std::vector<rbac::Group> groups() const override {
        return groups_;
    }

    [[nodiscard]] std::vector<std::string> group_ids_for_user(const std::string &user_id) const override {
        for (const auto &[stored_user_id, group_ids] : memberships_) {
            if (stored_user_id == user_id) {
                return group_ids;
            }
        }
        return {};
    }

    [[nodiscard]] std::vector<rbac::Policy> policies() const override {
        return policies_;
    }

    void save_group(const rbac::Group &group) override {
        groups_.push_back(group);
    }

    void add_user_to_group(const std::string &user_id, const std::string &group_id) override {
        for (auto &[stored_user_id, group_ids] : memberships_) {
            if (stored_user_id == user_id) {
                group_ids.push_back(group_id);
                return;
            }
        }
        memberships_.push_back({user_id, {group_id}});
    }

    void save_policy(const rbac::Policy &policy) override {
        policies_.push_back(policy);
    }

private:
    std::vector<rbac::Group> groups_;
    std::vector<std::pair<std::string, std::vector<std::string>>> memberships_;
    std::vector<rbac::Policy> policies_;
};

auth::SessionContext authenticated_user() {
    auth::SessionContext ctx;
    ctx.user_id = "u1";
    ctx.username = "alice";
    ctx.is_authenticated = true;
    return ctx;
}

rbac::Policy policy(rbac::SubjectType subject_type,
                    const std::string &subject_id,
                    const std::string &database_name,
                    const std::string &table_name,
                    const rbac::Permission permission,
                    const bool allowed) {
    rbac::Policy result;
    result.subject_type = subject_type;
    result.subject_id = subject_id;
    result.database_name = database_name;
    result.table_name = table_name;
    result.permission = permission;
    result.allowed = allowed;
    return result;
}

} // namespace

TEST(AccessManagerService, RejectsAnonymousSession) {
    InMemoryAccessControlStorage storage;
    rbac::PermissionResolver resolver;
    rbac::AccessManager manager(resolver, storage);

    auth::SessionContext ctx;

    EXPECT_FALSE(manager.allow(ctx, "db1", "users", rbac::Permission::ReadTable));
}

TEST(AccessManagerService, AllowsUserPermissionFromStorage) {
    InMemoryAccessControlStorage storage;
    storage.save_policy(policy(rbac::SubjectType::User, "u1", "db1", "users", rbac::Permission::ReadTable, true));
    rbac::PermissionResolver resolver;
    rbac::AccessManager manager(resolver, storage);

    EXPECT_TRUE(manager.allow(authenticated_user(), "db1", "users", rbac::Permission::ReadTable));
}

TEST(AccessManagerService, UsesGroupMembershipsFromStorage) {
    InMemoryAccessControlStorage storage;
    storage.add_user_to_group("u1", "analysts");
    storage.save_policy(policy(rbac::SubjectType::Group,
                               "analysts",
                               "db1",
                               "reports",
                               rbac::Permission::ReadTable,
                               true));
    rbac::PermissionResolver resolver;
    rbac::AccessManager manager(resolver, storage);

    EXPECT_TRUE(manager.allow(authenticated_user(), "db1", "reports", rbac::Permission::ReadTable));
}

TEST(AccessManagerService, AdminBypassesMissingPolicies) {
    InMemoryAccessControlStorage storage;
    rbac::PermissionResolver resolver;
    rbac::AccessManager manager(resolver, storage);

    auto ctx = authenticated_user();
    ctx.is_admin = true;

    EXPECT_TRUE(manager.allow(ctx, "db1", "private_table", rbac::Permission::DropTable));
}
