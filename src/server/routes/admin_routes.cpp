#include "server/routes/admin_routes.hpp"

#include "crow/json.h"

#include <stdexcept>

namespace server {
namespace {

rbac::Permission parse_permission(const std::string &value) {
    if (value == "read_table") return rbac::Permission::ReadTable;
    if (value == "write_table") return rbac::Permission::WriteTable;
    if (value == "create_table") return rbac::Permission::CreateTable;
    if (value == "drop_table") return rbac::Permission::DropTable;
    if (value == "create_database") return rbac::Permission::CreateDatabase;
    if (value == "drop_database") return rbac::Permission::DropDatabase;
    if (value == "manage_users") return rbac::Permission::ManageUsers;
    if (value == "grant_permissions") return rbac::Permission::GrantPermissions;
    throw std::runtime_error("unknown permission");
}

rbac::SubjectType parse_subject_type(const std::string &value) {
    if (value == "user") return rbac::SubjectType::User;
    if (value == "group") return rbac::SubjectType::Group;
    if (value == "default") return rbac::SubjectType::Default;
    throw std::runtime_error("unknown subject type");
}

bool is_allowed(crow::App<AccessLogMiddleware, AuthMiddleware> &app,
                const crow::request &req,
                rbac::AccessManager &access_manager,
                const rbac::Permission permission) {
    const auto &session = app.get_context<AuthMiddleware>(req).session;
    return access_manager.allow(session, "", "", permission);
}

} // namespace

void register_admin_routes(crow::App<AccessLogMiddleware, AuthMiddleware> &app,
                           services::AdminService &admin_service,
                           rbac::AccessManager &access_manager) {
    CROW_ROUTE(app, "/admin/groups").methods(crow::HTTPMethod::POST)(
        [&app, &admin_service, &access_manager](const crow::request &req) {
            if (!is_allowed(app, req, access_manager, rbac::Permission::ManageUsers)) {
                return crow::response(crow::status::FORBIDDEN, "Forbidden");
            }

            const auto body = crow::json::load(req.body);
            if (!body || !body.has("name")) {
                return crow::response(crow::status::BAD_REQUEST, "Invalid request payload");
            }

            try {
                return crow::response(crow::status::CREATED,
                                      admin_service.create_group(body["name"].s()));
            } catch (const std::exception &e) {
                return crow::response(crow::status::BAD_REQUEST, e.what());
            }
        });

    CROW_ROUTE(app, "/admin/groups/<string>/users").methods(crow::HTTPMethod::POST)(
        [&app, &admin_service, &access_manager](const crow::request &req, const std::string &group_id) {
            if (!is_allowed(app, req, access_manager, rbac::Permission::ManageUsers)) {
                return crow::response(crow::status::FORBIDDEN, "Forbidden");
            }

            const auto body = crow::json::load(req.body);
            if (!body || !body.has("user_id")) {
                return crow::response(crow::status::BAD_REQUEST, "Invalid request payload");
            }

            try {
                return crow::response(crow::status::OK,
                                      admin_service.add_user_to_group(body["user_id"].s(), group_id));
            } catch (const std::exception &e) {
                return crow::response(crow::status::BAD_REQUEST, e.what());
            }
        });

    CROW_ROUTE(app, "/admin/permissions/grant").methods(crow::HTTPMethod::POST)(
        [&app, &admin_service, &access_manager](const crow::request &req) {
            if (!is_allowed(app, req, access_manager, rbac::Permission::GrantPermissions)) {
                return crow::response(crow::status::FORBIDDEN, "Forbidden");
            }

            const auto body = crow::json::load(req.body);
            if (!body || !body.has("subject_type") || !body.has("subject_id") || !body.has("database_name")
                || !body.has("table_name") || !body.has("permission")) {
                return crow::response(crow::status::BAD_REQUEST, "Invalid request payload");
            }

            try {
                return crow::response(crow::status::OK,
                                      admin_service.grant_permission(parse_subject_type(body["subject_type"].s()),
                                                                     body["subject_id"].s(),
                                                                     body["database_name"].s(),
                                                                     body["table_name"].s(),
                                                                     parse_permission(body["permission"].s())));
            } catch (const std::exception &e) {
                return crow::response(crow::status::BAD_REQUEST, e.what());
            }
        });

    CROW_ROUTE(app, "/admin/permissions/revoke").methods(crow::HTTPMethod::POST)(
        [&app, &admin_service, &access_manager](const crow::request &req) {
            if (!is_allowed(app, req, access_manager, rbac::Permission::GrantPermissions)) {
                return crow::response(crow::status::FORBIDDEN, "Forbidden");
            }

            const auto body = crow::json::load(req.body);
            if (!body || !body.has("subject_type") || !body.has("subject_id") || !body.has("database_name")
                || !body.has("table_name") || !body.has("permission")) {
                return crow::response(crow::status::BAD_REQUEST, "Invalid request payload");
            }

            try {
                return crow::response(crow::status::OK,
                                      admin_service.revoke_permission(parse_subject_type(body["subject_type"].s()),
                                                                      body["subject_id"].s(),
                                                                      body["database_name"].s(),
                                                                      body["table_name"].s(),
                                                                      parse_permission(body["permission"].s())));
            } catch (const std::exception &e) {
                return crow::response(crow::status::BAD_REQUEST, e.what());
            }
        });
}

} // namespace server
