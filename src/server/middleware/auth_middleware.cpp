#include "server/middleware/auth_middleware.hpp"

namespace server {
namespace {

bool is_public_route(const std::string &url) {
    return url == "/login" || url == "/register" || url == "/metrics";
}

} // namespace

    AuthMiddleware::AuthMiddleware(auth::AuthService &auth_service)
        : auth_service_(auth_service) {
    }

    void AuthMiddleware::before_handle(crow::request &req, crow::response &res, context &ctx) const {
        if (is_public_route(req.url)) {
            return;
        }

        auto auth_header = req.get_header_value("Authorization");
        if (auth_header.empty() || auth_header.find("Bearer ") != 0) {
            res.code = crow::status::UNAUTHORIZED;
            res.body = "Missing or invalid Authorization header";
            res.end();
            return;
        }

        auto token = auth_header.substr(7); // Skip "Bearer "
        try {
            ctx.session = auth_service_.authenticate(token);
        } catch (const std::exception &e) {
            res.code = crow::status::UNAUTHORIZED;
            res.body = e.what();
            res.end();
        }
    }

    void AuthMiddleware::after_handle(crow::request &, crow::response &, context &) const {
        // No-op for now.
    }

} // namespace server
