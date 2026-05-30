#pragma once

namespace rbac {

enum class Permission {
    ReadTable,
    WriteTable,
    CreateTable,
    DropTable,
    CreateDatabase,
    DropDatabase,
    ManageUsers,
    GrantPermissions
};

} // namespace rbac
