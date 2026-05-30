#pragma once

#include "rbac/group.hpp"
#include "rbac/policy.hpp"

#include <string>
#include <vector>

namespace rbac {

class AccessControlStorage {
public:
    virtual ~AccessControlStorage() = default;

    [[nodiscard]] virtual std::vector<Group> groups() const = 0;
    [[nodiscard]] virtual std::vector<std::string> group_ids_for_user(const std::string &user_id) const = 0;
    [[nodiscard]] virtual std::vector<Policy> policies() const = 0;

    virtual void save_group(const Group &group) = 0;
    virtual void add_user_to_group(const std::string &user_id, const std::string &group_id) = 0;
    virtual void save_policy(const Policy &policy) = 0;
};

} // namespace rbac
