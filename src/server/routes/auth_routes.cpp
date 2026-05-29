#include "server/routes/auth_routes.hpp"

#include "crow/json.h"

namespace server {

    void register_auth_routes(crow::App<AccessLogMiddleware, AuthMiddleware> &app,
                              auth::AuthService &auth_service) {
        // POST /login
        CROW_ROUTE(app, "/login").methods(crow::HTTPMethod::POST)(
            [&auth_service](const crow::request &req) {
                auto body = crow::json::load(req.body);
                if (!body || !body.has("username") || !body.has("password")) {
                    return crow::response(crow::status::BAD_REQUEST, "Invalid request payload");
                }

                try {
                    auto token = auth_service.login(body["username"].s(), body["password"].s());
                    crow::json::wvalue response_body;
                    response_body["token"] = token;
                    return crow::response(crow::status::OK, response_body);
                } catch (const std::exception &e) {
                    return crow::response(crow::status::UNAUTHORIZED, e.what());
                }
            });

        // POST /register
        CROW_ROUTE(app, "/register").methods(crow::HTTPMethod::POST)(
            [&auth_service](const crow::request &req) {
                auto body = crow::json::load(req.body);
                if (!body || !body.has("username") || !body.has("password")) {
                    return crow::response(crow::status::BAD_REQUEST, "Invalid request payload");
                }

                try {
                    auto token = auth_service.register_user(body["username"].s(), body["password"].s());
                    crow::json::wvalue response_body;
                    response_body["token"] = token;
                    return crow::response(crow::status::CREATED, response_body);
                } catch (const std::exception &e) {
                    return crow::response(crow::status::BAD_REQUEST, e.what());
                }
            });
    }

} // namespace server
