#include "server/middleware/auth_middleware.hpp"

#include "common/not_implemented.hpp"

namespace server {

AuthMiddleware::AuthMiddleware(auth::AuthService &auth_service)
    : auth_service_(auth_service) {
}

void AuthMiddleware::before_handle(crow::request &req, crow::response &res, context &ctx) const {
    common::not_implemented();
}

void AuthMiddleware::after_handle(crow::request &, crow::response &, context &) const {
    common::not_implemented();
}

} // namespace server
