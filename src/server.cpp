#include "async/task_manager.hpp"
#include "crow.h"
#include "core/dbms.hpp"
#include "query/executor.hpp"
#include "query/query_runner.hpp"
#include "server/middleware/access_logger.hpp"
#include "server/middleware/telemetry.hpp"
#include "storage/storage_manager.hpp"
#include <cerrno>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>

namespace {

crow::response json_response(const int status_code, const nlohmann::json& body) {
    crow::response response(status_code, body.dump());
    response.set_header("Content-Type", "application/json");
    return response;
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
    TaskManager tasks([&exec, &storage, &dbms, &mtx](const std::string& sql) {
        std::lock_guard lock(mtx);

        try {
            auto output = run_sql(sql, exec);
            storage.save(dbms);
            return output.empty() ? "OK" : output;
        } catch (...) {
            storage.save(dbms);
            throw;
        }
    });

    CROW_ROUTE(app, "/query").methods(crow::HTTPMethod::Post)(
        [&tasks](const crow::request &req) {
            const auto id = tasks.submit(req.body);

            return json_response(
                202,
                {
                    {"task_id", id},
                    {"status", "pending"}
                });
        });

    CROW_ROUTE(app, "/task/<string>").methods(crow::HTTPMethod::Get)(
        [&tasks](const std::string& id) {
            const auto task = tasks.get_task(id);

            if (!task.has_value()) {
                return json_response(
                    404,
                    {
                        {"error", "task not found"},
                        {"task_id", id}
                    });
            }

            nlohmann::json body{
                {"task_id", task->id},
                {"status", task_status_to_string(task->status)}
            };

            if (task->status == TaskStatus::Done) {
                body["result"] = task->result;
            } else if (task->status == TaskStatus::Error) {
                body["error"] = task->error;
            }

            return json_response(200, body);
        });

    CROW_ROUTE(app, "/metrics").methods(crow::HTTPMethod::Get)(
        [telemetry]() {
            crow::response response(telemetry->snapshot_json());
            response.code = 200;
            response.set_header("Content-Type", "application/json");
            return response;
        });
    
    CROW_ROUTE(app, "/heartbeat").methods(crow::HTTPMethod::Get)(
        [] {
            return crow::response(200, "OK");
        });

    std::cout << "dbms_server listening on port " << port
            << ", data dir: " << data_dir
            << ", access log: " << access_log_path << "\n";
    app.port(port).multithreaded().run();
}
