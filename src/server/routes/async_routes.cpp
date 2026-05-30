#include "server/routes/async_routes.hpp"

#include "query/query_runner.hpp"

#include <nlohmann/json.hpp>
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

TaskManager make_task_manager(Executor &exec, StorageManager &storage, DBMS &dbms, std::mutex &mtx) {
    return TaskManager([&exec, &storage, &dbms, &mtx](const std::string &sql) {
        const auto statements = parse_sql(sql);
        std::lock_guard lock(mtx);
        try {
            const auto output = run_statements(statements, exec);
            storage.save(dbms);
            return output.empty() ? std::string{"OK"} : output;
        } catch (...) {
            storage.save(dbms);
            throw;
        }
    });
}

void register_async_routes(crow::App<AccessLogMiddleware, AuthMiddleware> &app,
                           TaskManager &task_manager,
                           rbac::AccessManager &access_manager) {
    CROW_ROUTE(app, "/async/query").methods(crow::HTTPMethod::Post)(
        [&app, &task_manager, &access_manager](const crow::request &req) {
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

                const auto task_id = task_manager.submit(req.body);
                return crow::response(202, nlohmann::json{{"task_id", task_id}}.dump());
            } catch (const std::exception &e) {
                return crow::response(400, e.what());
            }
        });

    CROW_ROUTE(app, "/task/<string>").methods(crow::HTTPMethod::Get)(
        [&task_manager](const std::string &task_id) {
            const auto task = task_manager.get_task(task_id);
            if (!task.has_value()) {
                return crow::response(404, "task not found");
            }

            nlohmann::json body{{"task_id", task->id},
                                {"status", task_status_to_string(task->status)}};
            if (!task->result.empty()) {
                body["result"] = task->result;
            }
            if (!task->error.empty()) {
                body["error"] = task->error;
            }
            return crow::response(200, body.dump());
        });
}

} // namespace server
