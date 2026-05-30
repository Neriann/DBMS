#pragma once

#include "auth/account_storage.hpp"
#include "rbac/access_control_storage.hpp"

#include <filesystem>

namespace auth {

class FileAccountStorage final : public AccountStorage, public rbac::AccessControlStorage {
public:
    explicit FileAccountStorage(std::filesystem::path data_dir);

    [[nodiscard]] std::optional<User> find_by_username(const std::string &username) const override;
    [[nodiscard]] std::optional<User> find_by_id(const std::string &user_id) const override;
    [[nodiscard]] bool has_users() const override;
    void save_user(const User &user) override;

    [[nodiscard]] std::vector<rbac::Group> groups() const override;
    [[nodiscard]] std::vector<std::string> group_ids_for_user(const std::string &user_id) const override;
    [[nodiscard]] std::vector<rbac::Policy> policies() const override;

    void save_group(const rbac::Group &group) override;
    void add_user_to_group(const std::string &user_id, const std::string &group_id) override;
    void save_policy(const rbac::Policy &policy) override;

private:
    [[nodiscard]] std::filesystem::path users_path() const;
    [[nodiscard]] std::filesystem::path groups_path() const;
    [[nodiscard]] std::filesystem::path user_groups_path() const;
    [[nodiscard]] std::filesystem::path permissions_path() const;

    std::filesystem::path data_dir_;
};

} // namespace auth
