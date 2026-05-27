#pragma once

#include "auth/auth_service.hpp"
#include "crow.h"
#include "server/middleware/access_logger.hpp"
#include "server/middleware/auth_middleware.hpp"

namespace server {

void register_auth_routes(crow::App<AccessLogMiddleware, AuthMiddleware> &app,
                          auth::AuthService &auth_service);

} // namespace server

