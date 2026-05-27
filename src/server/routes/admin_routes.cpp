#include "server/routes/admin_routes.hpp"

#include "common/not_implemented.hpp"

namespace server {

void register_admin_routes(crow::App<AccessLogMiddleware, AuthMiddleware> &app,
                           services::AdminService &admin_service) {
    common::not_implemented(app, admin_service);
}

} // namespace server

