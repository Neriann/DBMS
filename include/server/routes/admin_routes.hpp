#pragma once

#include "crow.h"
#include "rbac/access_manager.hpp"
#include "server/middleware/access_logger.hpp"
#include "server/middleware/auth_middleware.hpp"
#include "services/admin_service.hpp"

namespace server {

void register_admin_routes(crow::App<AccessLogMiddleware, AuthMiddleware> &app,
                           services::AdminService &admin_service,
                           rbac::AccessManager &access_manager);

} // namespace server
