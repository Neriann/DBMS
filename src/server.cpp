#include "crow.h"
#include "core/dbms.hpp"
#include "query/executor.hpp"
#include "query/query_runner.hpp"
#include "server/access_logger.hpp"
#include "storage/storage_manager.hpp"
#include <cerrno>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>

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
    storage.load(dbms);
    Executor exec(dbms);
    std::mutex mtx;
    const auto access_log_path = std::filesystem::path(data_dir) / "access.log";
    std::shared_ptr<AccessLogger> access_logger;
    try {
        access_logger = std::make_shared<AccessLogger>(access_log_path);
    } catch (const std::exception &e) {
        std::cerr << "failed to initialize access logging: " << e.what() << "\n";
        return 1;
    }
    auto telemetry = std::make_shared<TelemetryCollector>();
    crow::App<AccessLogMiddleware> app(AccessLogMiddleware{access_logger, telemetry});

    CROW_ROUTE(app, "/query").methods(crow::HTTPMethod::Post)(
        [&exec, &storage, &dbms, &mtx](const crow::request &req) {
            try {
                std::lock_guard lock(mtx);
                std::string output;
                try {
                    output = run_sql(req.body, exec);
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

    CROW_ROUTE(app, "/metrics").methods(crow::HTTPMethod::Get)(
        [telemetry]() {
            crow::response response(telemetry->snapshot_json());
            response.code = 200;
            response.set_header("Content-Type", "application/json");
            return response;
        });

    std::cout << "dbms_server listening on port " << port
            << ", data dir: " << data_dir
            << ", access log: " << access_log_path << "\n";
    app.port(port).multithreaded().run();
}
