#pragma once

#include "rbac/permission.hpp"

#include <string>

namespace rbac {

enum class SubjectType {
    User,
    Group,
    Default
};

struct Policy {
    SubjectType subject_type = SubjectType::Default;
    std::string subject_id;
    std::string database_name;
    std::string table_name;
    Permission permission = Permission::ReadTable;
    bool allowed = false;
};

} // namespace rbac

