#pragma once

#include "rbac/permission.hpp"

#include <string>
#include <vector>

namespace rbac {

struct Role {
    std::string name;
    std::vector<Permission> permissions;
};

} // namespace rbac

