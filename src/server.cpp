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
#include "services/admin_service.hpp"
#include "server/routes/async_routes.hpp"
#include "storage/storage_manager.hpp"
#include <cerrno>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

namespace {

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
    auto task_manager = server::make_task_manager(exec, storage, dbms, mtx);
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
    server::register_async_routes(app, task_manager, access_manager);

    std::cout << "dbms_server listening on port " << port
            << ", data dir: " << data_dir
            << ", access log: " << access_log_path << "\n";
    app.port(port).multithreaded().run();
}
