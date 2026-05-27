#include "server/routes/auth_routes.hpp"

#include "common/not_implemented.hpp"

namespace server {

void register_auth_routes(crow::App<AccessLogMiddleware, AuthMiddleware> &app,
                          auth::AuthService &auth_service) {
    common::not_implemented();
}

} // namespace server
