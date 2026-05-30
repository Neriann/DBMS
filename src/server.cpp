#include "async/task_manager.hpp"
#include "crow.h"
#include "auth/auth_service.hpp"
#include "auth/file_account_storage.hpp"
#include "auth/jwt_service.hpp"
#include "auth/password_hasher.hpp"
#include "core/dbms.hpp"
#include "query/executor.hpp"
#include "rbac/access_manager.hpp"
#include "rbac/permission_resolver.hpp"
#include "server/routes/admin_routes.hpp"
#include "server/routes/auth_routes.hpp"
#include "server/routes/metrics_routes.hpp"
#include "server/routes/query_routes.hpp"
#include "server/middleware/access_logger.hpp"
#include "server/middleware/telemetry.hpp"
#include "query/query_runner.hpp"
#include "services/admin_service.hpp"
#include "storage/storage_manager.hpp"
#include <cerrno>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <variant>

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

std::string generate_jwt_secret(const auth::PasswordHasher &password_hasher) {
    std::string secret;
    for (int i = 0; i < 4; ++i) { // generate 512-bit secret
        secret += password_hasher.generate_salt();
    }
    return secret;
}

std::string read_or_create_jwt_secret(const std::filesystem::path &data_dir,
                                      const auth::PasswordHasher &password_hasher) {
    std::filesystem::create_directories(data_dir);

    const auto secret_path = data_dir / "jwt_secret";
    if (std::filesystem::exists(secret_path)) {
        std::ifstream in(secret_path);
        if (!in) {
            throw std::runtime_error("failed to open JWT secret file: " + secret_path.string());
        }

        std::string secret;
        std::getline(in, secret);
        if (secret.empty()) {
            throw std::runtime_error("JWT secret file is empty: " + secret_path.string());
        }
        return secret;
    }

    const auto secret = generate_jwt_secret(password_hasher);
    std::ofstream out(secret_path, std::ios::trunc);
    if (!out) {
        throw std::runtime_error("failed to create JWT secret file: " + secret_path.string());
    }
    out << secret << '\n';
    return secret;
}

} // namespace

int main(const int argc, char **argv) {
    std::uint16_t port = 8080;
    std::string data_dir = "./data";

    if (argc > 1) {
        errno = 0;
        char *end = nullptr;
        const auto parsed_port = std::strtol(argv[1], &end, 10);
        if (errno != 0 || end == argv[1] || *end != '\0' || parsed_port <= 0 || parsed_port > 65535) {
            std::cerr << "usage: " << argv[0] << " [port] [data_dir]\n";
            return 1;
        }
        port = static_cast<std::uint16_t>(parsed_port);
    }
    if (argc > 2) {
        data_dir = argv[2];
    }
    if (argc > 3) {
        std::cerr << "usage: " << argv[0] << " [port] [data_dir]\n";
        return 1;
    }

    DBMS dbms;
    StorageManager storage(data_dir);
    auth::FileAccountStorage account_storage(data_dir);
    auth::PasswordHasher password_hasher;
    auth::JwtService jwt(read_or_create_jwt_secret(data_dir, password_hasher));
    auth::AuthService auth_service(account_storage, jwt, password_hasher);
    rbac::PermissionResolver permission_resolver;
    rbac::AccessManager access_manager(permission_resolver, account_storage);
    services::AdminService admin_service(account_storage);

    storage.load(dbms);
    Executor exec(dbms);
    std::mutex mtx;
    TaskManager task_manager([&exec, &storage, &dbms, &mtx](const std::string &sql) {
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
    const auto access_log_path = std::filesystem::path(data_dir) / "access.log";
    std::shared_ptr<AccessLogger> access_logger;
    try {
        access_logger = std::make_shared<AccessLogger>(access_log_path);
    } catch (const std::exception &e) {
        std::cerr << "failed to initialize access logging: " << e.what() << "\n";
        return 1;
    }
    auto telemetry = std::make_shared<TelemetryCollector>();
    crow::App<AccessLogMiddleware, server::AuthMiddleware> app(AccessLogMiddleware{access_logger, telemetry},
                                                               server::AuthMiddleware{auth_service});

    server::register_auth_routes(app, auth_service);
    server::register_admin_routes(app, admin_service, access_manager);
    server::register_query_routes(app, exec, storage, dbms, mtx, access_manager);
    server::register_metrics_routes(app, telemetry);

    CROW_ROUTE(app, "/heartbeat").methods(crow::HTTPMethod::Get)([] {
        return crow::response(200, "OK");
    });

    CROW_ROUTE(app, "/async/query").methods(crow::HTTPMethod::Post)(
        [&app, &task_manager, &access_manager](const crow::request &req) {
            try {
                const auto statements = parse_sql(req.body);
                const auto &session = app.get_context<server::AuthMiddleware>(req).session;
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

    std::cout << "dbms_server listening on port " << port
            << ", data dir: " << data_dir
            << ", access log: " << access_log_path << "\n";
    app.port(port).multithreaded().run();
}
