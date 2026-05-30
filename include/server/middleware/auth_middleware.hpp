#pragma once

#include "auth/auth_service.hpp"
#include "auth/session_context.hpp"
#include "crow.h"

namespace server {

struct AuthMiddleware {
    struct context {
        auth::SessionContext session;
    };

    explicit AuthMiddleware(auth::AuthService &auth_service);

    void before_handle(crow::request &req, crow::response &res, context &ctx) const;
    void after_handle(crow::request &req, crow::response &res, context &ctx) const;

private:
    auth::AuthService &auth_service_;
};

} // namespace server
