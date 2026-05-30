#include "server/routes/query_routes.hpp"

#include "query/query_runner.hpp"

#include <exception>
#include <string>
#include <variant>

namespace server {
namespace {

template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

struct RequiredPermission {
    std::string database_name;
    std::string table_name;
    rbac::Permission permission;
};

RequiredPermission required_permission(const Statement &stmt) {
    return std::visit(
        Overloaded{
            [](const CreateDatabaseStmt &s) {
                return RequiredPermission{s.db_name, "", rbac::Permission::CreateDatabase};
            },
            [](const DropDatabaseStmt &s) {
                return RequiredPermission{s.db_name, "", rbac::Permission::DropDatabase};
            },
            [](const UseStmt &s) {
                return RequiredPermission{s.db_name, "", rbac::Permission::ReadTable};
            },
            [](const CreateTableStmt &s) {
                return RequiredPermission{s.db_name, s.table_name, rbac::Permission::CreateTable};
            },
            [](const DropTableStmt &s) {
                return RequiredPermission{s.db_name, s.table_name, rbac::Permission::DropTable};
            },
            [](const InsertStmt &s) {
                return RequiredPermission{s.db_name, s.table_name, rbac::Permission::WriteTable};
            },
            [](const UpdateStmt &s) {
                return RequiredPermission{s.db_name, s.table_name, rbac::Permission::WriteTable};
            },
            [](const DeleteStmt &s) {
                return RequiredPermission{s.db_name, s.table_name, rbac::Permission::WriteTable};
            },
            [](const SelectStmt &s) {
                return RequiredPermission{s.db_name, s.table_name, rbac::Permission::ReadTable};
            },
            [](const RevertStmt &s) {
                return RequiredPermission{s.db_name, s.table_name, rbac::Permission::WriteTable};
            }},
        stmt);
}

} // namespace

void register_query_routes(crow::App<AccessLogMiddleware, AuthMiddleware> &app,
                           Executor &exec,
                           StorageManager &storage,
                           DBMS &dbms,
                           std::mutex &mtx,
                           rbac::AccessManager &access_manager) {
    CROW_ROUTE(app, "/query").methods(crow::HTTPMethod::Post)(
        [&app, &exec, &storage, &dbms, &mtx, &access_manager](const crow::request &req) {
            try {
                const auto statements = parse_sql(req.body);
                const auto &session = app.get_context<AuthMiddleware>(req).session;
                for (const Statement &stmt : statements) {
                    const auto required = required_permission(stmt);
                    if (!access_manager.allow(session,
                                              required.database_name,
                                              required.table_name,
                                              required.permission)) {
                        return crow::response(crow::status::FORBIDDEN, "Forbidden");
                    }
                }

                std::lock_guard lock(mtx);
                std::string output;
                try {
                    output = run_statements(statements, exec);
                } catch (...) {
                    storage.save(dbms);
                    throw;
                }
                storage.save(dbms);
                return crow::response(200, output.empty() ? "OK" : output);
            } catch (const std::exception &e) {
                return crow::response(400, e.what());
            }
        });
}

} // namespace server
