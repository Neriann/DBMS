#include "auth/file_account_storage.hpp"

#include <algorithm>
#include <filesystem>
#include <gtest/gtest.h>

namespace {

std::filesystem::path temp_storage_dir(const std::string &name) {
    const auto path = std::filesystem::temp_directory_path() / ("dbms_" + name);
    std::filesystem::remove_all(path);
    return path;
}

} // namespace

TEST(FileAccountStorage, PersistsUsersAcrossInstances) {
    const auto dir = temp_storage_dir("users_storage_test");
    auth::FileAccountStorage storage(dir);

    auth::User user;
    user.id = "u1";
    user.username = "alice";
    user.password_hash = "hash";
    user.salt = "salt";
    user.is_admin = true;
    storage.save_user(user);

    auth::FileAccountStorage reloaded(dir);
    const auto by_username = reloaded.find_by_username("alice");
    const auto by_id = reloaded.find_by_id("u1");

    ASSERT_TRUE(by_username.has_value());
    ASSERT_TRUE(by_id.has_value());
    EXPECT_EQ(by_username->id, "u1");
    EXPECT_TRUE(by_id->is_admin);

    std::filesystem::remove_all(dir);
}

TEST(FileAccountStorage, PersistsGroupsMembershipsAndPolicies) {
    const auto dir = temp_storage_dir("rbac_storage_test");
    auth::FileAccountStorage storage(dir);

    storage.save_group({"g1", "analysts"});
    storage.add_user_to_group("u1", "g1");

    rbac::Policy policy;
    policy.subject_type = rbac::SubjectType::Group;
    policy.subject_id = "g1";
    policy.database_name = "db1";
    policy.table_name = "reports";
    policy.permission = rbac::Permission::ReadTable;
    policy.allowed = true;
    storage.save_policy(policy);

    auth::FileAccountStorage reloaded(dir);

    ASSERT_EQ(reloaded.groups().size(), 1U);
    EXPECT_EQ(reloaded.groups()[0].name, "analysts");
    EXPECT_EQ(reloaded.group_ids_for_user("u1"), std::vector<std::string>{"g1"});

    const auto policies = reloaded.policies();
    const auto it = std::ranges::find_if(policies, [](const rbac::Policy &stored_policy) {
        return stored_policy.subject_type == rbac::SubjectType::Group
            && stored_policy.subject_id == "g1"
            && stored_policy.database_name == "db1"
            && stored_policy.table_name == "reports"
            && stored_policy.permission == rbac::Permission::ReadTable;
    });
    ASSERT_NE(it, policies.end());
    EXPECT_TRUE(it->allowed);

    std::filesystem::remove_all(dir);
}

TEST(FileAccountStorage, SavePolicyOverwritesAllowedFlag) {
    const auto dir = temp_storage_dir("policy_overwrite_test");
    auth::FileAccountStorage storage(dir);

    rbac::Policy policy;
    policy.subject_type = rbac::SubjectType::User;
    policy.subject_id = "u1";
    policy.database_name = "db1";
    policy.table_name = "users";
    policy.permission = rbac::Permission::ReadTable;
    policy.allowed = true;
    storage.save_policy(policy);

    policy.allowed = false;
    storage.save_policy(policy);

    const auto policies = storage.policies();
    const auto it = std::ranges::find_if(policies, [](const rbac::Policy &stored_policy) {
        return stored_policy.subject_type == rbac::SubjectType::User
            && stored_policy.subject_id == "u1"
            && stored_policy.database_name == "db1"
            && stored_policy.table_name == "users"
            && stored_policy.permission == rbac::Permission::ReadTable;
    });
    ASSERT_NE(it, policies.end());
    EXPECT_FALSE(it->allowed);

    std::filesystem::remove_all(dir);
}

TEST(FileAccountStorage, CreatesDefaultReadPermissionInPermissionsTable) {
    const auto dir = temp_storage_dir("default_read_policy_test");
    auth::FileAccountStorage storage(dir);

    const auto policies = storage.policies();
    const auto it = std::ranges::find_if(policies, [](const rbac::Policy &policy) {
        return policy.subject_type == rbac::SubjectType::Default
            && policy.subject_id.empty()
            && policy.database_name == "*"
            && policy.table_name == "*"
            && policy.permission == rbac::Permission::ReadTable;
    });

    ASSERT_NE(it, policies.end());
    EXPECT_TRUE(it->allowed);

    std::filesystem::remove_all(dir);
}
