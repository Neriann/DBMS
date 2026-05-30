#include "auth/auth_service.hpp"
#include "auth/file_account_storage.hpp"
#include "auth/jwt_service.hpp"
#include "auth/password_hasher.hpp"
#include "cluster/cluster_manager.hpp"
#include "core/dbms.hpp"
#include "distributed/execution/distributed_executor.hpp"
#include "distributed/execution/result_merger.hpp"
#include "distributed/execution/scatter_gather.hpp"
#include "distributed/routing/query_router.hpp"
#include "distributed/routing/shard_resolver.hpp"
#include "query/executor.hpp"
#include "rbac/access_manager.hpp"
#include "server/cluster_heartbeat_service.hpp"
#include "server/middleware/access_logger.hpp"
#include "server/middleware/auth_middleware.hpp"
#include "server/routes/admin_routes.hpp"
#include "server/routes/async_routes.hpp"
#include "server/routes/auth_routes.hpp"
#include "server/routes/entrypoint_routes.hpp"
#include "server/routes/query_routes.hpp"
#include "services/admin_service.hpp"
#include "storage/storage_manager.hpp"

#include "crow/json.h"

#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <filesystem>
#include <gtest/gtest.h>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

using AuthApp = crow::App<AccessLogMiddleware, server::AuthMiddleware>;

struct HttpResponse {
    int code = 0;
    std::string body;
};

std::filesystem::path temp_dir(const std::string &name) {
    const auto path = std::filesystem::temp_directory_path() / ("dbms_app_demo_" + name);
    std::filesystem::remove_all(path);
    return path;
}

std::uint16_t next_port() {
    static std::atomic_uint16_t port{19400};
    return port.fetch_add(1);
}

HttpResponse http_request(const std::uint16_t port,
                          const std::string &method,
                          const std::string &url,
                          const std::string &body = "",
                          const std::string &content_type = "text/plain; charset=utf-8",
                          const std::string &token = "") {
    boost::asio::io_context io;
    boost::asio::ip::tcp::socket socket(io);
    socket.connect({boost::asio::ip::make_address("127.0.0.1"), port});

    std::ostringstream request;
    request << method << ' ' << url << " HTTP/1.1\r\n"
            << "Host: 127.0.0.1:" << port << "\r\n"
            << "Connection: close\r\n";
    if (!body.empty()) {
        request << "Content-Type: " << content_type << "\r\n"
                << "Content-Length: " << body.size() << "\r\n";
    }
    if (!token.empty()) {
        request << "Authorization: Bearer " << token << "\r\n";
    }
    request << "\r\n" << body;

    boost::asio::write(socket, boost::asio::buffer(request.str()));

    boost::asio::streambuf buffer;
    boost::system::error_code ec;
    std::ostringstream raw;
    while (boost::asio::read(socket, buffer, boost::asio::transfer_at_least(1), ec)) {
        raw << &buffer;
    }
    if (ec != boost::asio::error::eof) {
        throw boost::system::system_error(ec);
    }
    raw << &buffer;

    const auto text = raw.str();
    const auto status_end = text.find("\r\n");
    const auto body_start = text.find("\r\n\r\n");
    if (status_end == std::string::npos || body_start == std::string::npos) {
        throw std::runtime_error("invalid HTTP response: " + text);
    }

    std::istringstream status_line(text.substr(0, status_end));
    std::string http_version;
    HttpResponse response;
    status_line >> http_version >> response.code;
    response.body = text.substr(body_start + 4);
    return response;
}

void wait_until_ready(const std::uint16_t port) {
    for (int attempt = 0; attempt < 200; ++attempt) {
        try {
            boost::asio::io_context io;
            boost::asio::ip::tcp::socket socket(io);
            socket.connect({boost::asio::ip::make_address("127.0.0.1"), port});
            return;
        } catch (const boost::system::system_error &) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    throw std::runtime_error("test HTTP server did not start");
}

std::string json_string(const std::string &json, const std::string &key) {
    const auto body = crow::json::load(json);
    if (!body || !body.has(key)) {
        throw std::runtime_error("missing JSON key: " + key + " in " + json);
    }
    return body[key].s();
}

struct ServerAppFixture {
    explicit ServerAppFixture(const std::string &name)
        : dir(temp_dir(name)),
          port(next_port()),
          storage(dir),
          account_storage(dir),
          jwt("alabuga"),
          auth_service(account_storage, jwt, hasher),
          access_manager(permission_resolver, account_storage),
          admin_service(account_storage),
          executor(dbms),
          task_manager(server::make_task_manager(executor, storage, dbms, mtx)),
          logger(std::make_shared<AccessLogger>(dir / "access.log")),
          telemetry(std::make_shared<TelemetryCollector>()),
          app(AccessLogMiddleware{logger, telemetry}, server::AuthMiddleware{auth_service}) {
        storage.load(dbms);
        server::register_auth_routes(app, auth_service);
        server::register_admin_routes(app, admin_service, access_manager);
        server::register_query_routes(app, executor, storage, dbms, mtx, access_manager);
        server::register_async_routes(app, task_manager, access_manager);
        CROW_ROUTE(app, "/heartbeat").methods(crow::HTTPMethod::Get)([] {
            return crow::response(200, "OK");
        });

        thread = std::thread([this] {
            app.bindaddr("127.0.0.1").port(port).run();
        });
        wait_until_ready(port);
    }

    ~ServerAppFixture() {
        app.stop();
        if (thread.joinable()) {
            thread.join();
        }
        std::filesystem::remove_all(dir);
    }

    std::string register_admin() const {
        const auto response = http_request(port,
                                           "POST",
                                           "/register",
                                           "{\"username\":\"admin\",\"password\":\"secret\"}",
                                           "application/json");
        EXPECT_EQ(response.code, crow::status::CREATED) << response.body;
        return json_string(response.body, "token");
    }

    std::filesystem::path dir;
    std::uint16_t port;
    DBMS dbms;
    StorageManager storage;
    auth::FileAccountStorage account_storage;
    auth::PasswordHasher hasher;
    auth::JwtService jwt;
    auth::AuthService auth_service;
    rbac::PermissionResolver permission_resolver;
    rbac::AccessManager access_manager;
    services::AdminService admin_service;
    Executor executor;
    std::mutex mtx;
    TaskManager task_manager;
    std::shared_ptr<AccessLogger> logger;
    std::shared_ptr<TelemetryCollector> telemetry;
    AuthApp app;
    std::thread thread;
};

struct StorageHeartbeatFixture {
    StorageHeartbeatFixture() : port(next_port()) {
        CROW_ROUTE(app, "/heartbeat").methods(crow::HTTPMethod::Get)([] {
            return crow::response(200, "OK");
        });
        thread = std::thread([this] {
            app.bindaddr("127.0.0.1").port(port).run();
        });
        wait_until_ready(port);
    }

    ~StorageHeartbeatFixture() {
        stop();
    }

    void stop() {
        app.stop();
        if (thread.joinable()) {
            thread.join();
        }
    }

    std::uint16_t port;
    crow::SimpleApp app;
    std::thread thread;
};

struct EntrypointFixture {
    explicit EntrypointFixture(const std::string &name)
        : dir(temp_dir(name)),
          port(next_port()),
          storage(dir),
          cluster_manager(storage),
          resolver(cluster_manager),
          router(resolver),
          executor(router, scatter_gather, merger),
          heartbeat(cluster_manager, cluster_mtx, std::chrono::milliseconds(50)) {
        server::register_entrypoint_routes(app, cluster_manager, executor, cluster_mtx);
        heartbeat.start();
        thread = std::thread([this] {
            app.bindaddr("127.0.0.1").port(port).run();
        });
        wait_until_ready(port);
    }

    ~EntrypointFixture() {
        heartbeat.stop();
        app.stop();
        if (thread.joinable()) {
            thread.join();
        }
        std::filesystem::remove_all(dir);
    }

    std::filesystem::path dir;
    std::uint16_t port;
    cluster::ClusterStateStorage storage;
    cluster::ClusterManager cluster_manager;
    distributed::routing::ShardResolver resolver;
    distributed::routing::QueryRouter router;
    distributed::execution::ScatterGather scatter_gather;
    distributed::execution::ResultMerger merger;
    distributed::execution::DistributedExecutor executor;
    std::mutex cluster_mtx;
    server::ClusterHeartbeatService heartbeat;
    crow::SimpleApp app;
    std::thread thread;
};

} // namespace

TEST(ApplicationDemoIntegration, AsyncQueryLifecycleUsesJwtRbacRoutesAndStorage) {
    ServerAppFixture fixture("async_lifecycle");
    const auto token = fixture.register_admin();

    const auto submit = http_request(fixture.port,
                                     "POST",
                                     "/async/query",
                                     "CREATE DATABASE async_demo;",
                                     "text/plain; charset=utf-8",
                                     token);
    ASSERT_EQ(submit.code, 202) << submit.body;
    const auto task_id = json_string(submit.body, "task_id");

    HttpResponse status;
    for (int attempt = 0; attempt < 100; ++attempt) {
        status = http_request(fixture.port, "GET", "/task/" + task_id, "", "text/plain; charset=utf-8", token);
        ASSERT_EQ(status.code, 200) << status.body;
        if (status.body.find("\"status\":\"done\"") != std::string::npos) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_NE(status.body.find("\"status\":\"done\""), std::string::npos) << status.body;
    EXPECT_NE(status.body.find("Database 'async_demo' created"), std::string::npos) << status.body;
    EXPECT_TRUE(std::filesystem::is_directory(fixture.dir / "async_demo"));
}

TEST(ApplicationDemoIntegration, EntrypointNodeRoutesAndHeartbeatRemoveDeadStorageNode) {
    StorageHeartbeatFixture storage_node;
    EntrypointFixture entrypoint("entrypoint_heartbeat");

    const auto add = http_request(entrypoint.port,
                                  "POST",
                                  "/nodes",
                                  "{\"id\":\"node1\",\"host\":\"127.0.0.1\",\"port\":" + std::to_string(storage_node.port) + "}",
                                  "application/json");
    ASSERT_EQ(add.code, 201) << add.body;

    const auto listed = http_request(entrypoint.port, "GET", "/nodes");
    ASSERT_EQ(listed.code, 200) << listed.body;
    EXPECT_NE(listed.body.find("\"id\":\"node1\""), std::string::npos);

    const auto heartbeat = http_request(storage_node.port, "GET", "/heartbeat");
    EXPECT_EQ(heartbeat.code, 200);
    EXPECT_EQ(heartbeat.body, "OK");

    storage_node.stop();

    for (int attempt = 0; attempt < 100; ++attempt) {
        const auto nodes = http_request(entrypoint.port, "GET", "/nodes");
        ASSERT_EQ(nodes.code, 200) << nodes.body;
        if (nodes.body.find("node1") == std::string::npos) {
            SUCCEED();
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    FAIL() << "dead node was not removed from entrypoint topology";
}
