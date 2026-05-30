#include "auth/auth_service.hpp"
#include "auth/file_account_storage.hpp"
#include "auth/jwt_service.hpp"
#include "auth/password_hasher.hpp"
#include "core/dbms.hpp"
#include "query/executor.hpp"
#include "rbac/access_manager.hpp"
#include "server/middleware/access_logger.hpp"
#include "server/middleware/auth_middleware.hpp"
#include "server/routes/admin_routes.hpp"
#include "server/routes/auth_routes.hpp"
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

using TestApp = crow::App<AccessLogMiddleware, server::AuthMiddleware>;

struct HttpResponse {
    int code = 0;
    std::string body;
};

std::filesystem::path temp_storage_dir(const std::string &name) {
    const auto path = std::filesystem::temp_directory_path() / ("dbms_" + name);
    std::filesystem::remove_all(path);
    return path;
}

std::uint16_t next_port() {
    static std::atomic_uint16_t port{19080};
    return port.fetch_add(1);
}

HttpResponse http_post(const std::uint16_t port,
                       const std::string &url,
                       const std::string &body,
                       const std::string &content_type,
                       const std::string &token = "") {
    boost::asio::io_context io;
    boost::asio::ip::tcp::socket socket(io);
    socket.connect({boost::asio::ip::make_address("127.0.0.1"), port});

    std::ostringstream request;
    request << "POST " << url << " HTTP/1.1\r\n"
            << "Host: 127.0.0.1:" << port << "\r\n"
            << "Content-Type: " << content_type << "\r\n"
            << "Content-Length: " << body.size() << "\r\n"
            << "Connection: close\r\n";
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

    const auto response_text = raw.str();
    const auto status_end = response_text.find("\r\n");
    const auto body_start = response_text.find("\r\n\r\n");
    if (status_end == std::string::npos || body_start == std::string::npos) {
        throw std::runtime_error("invalid HTTP response: " + response_text);
    }

    std::istringstream status_line(response_text.substr(0, status_end));
    std::string http_version;
    HttpResponse response;
    status_line >> http_version >> response.code;
    response.body = response_text.substr(body_start + 4);
    return response;
}

std::string token_from(const HttpResponse &res) {
    const auto body = crow::json::load(res.body);
    if (!body || !body.has("token")) {
        throw std::runtime_error("response does not contain token: " + res.body);
    }
    return body["token"].s();
}

std::string user_id_from_tbl(const std::filesystem::path &dir, const std::string &username) {
    auth::FileAccountStorage storage(dir);
    const auto user = storage.find_by_username(username);
    if (!user.has_value()) {
        throw std::runtime_error("user not found: " + username);
    }
    return user->id;
}

struct RbacHttpFixture {
    explicit RbacHttpFixture(const std::string &test_name)
        : dir(temp_storage_dir(test_name)),
          port(next_port()),
          storage_manager(dir),
          account_storage(dir),
          jwt("alabuga"),
          auth_service(account_storage, jwt, hasher),
          access_manager(resolver, account_storage),
          admin_service(account_storage),
          executor(dbms),
          logger(std::make_shared<AccessLogger>(dir / "access.log")),
          telemetry(std::make_shared<TelemetryCollector>()),
          app(AccessLogMiddleware{logger, telemetry}, server::AuthMiddleware{auth_service}) {
        storage_manager.load(dbms);
        server::register_auth_routes(app, auth_service);
        server::register_admin_routes(app, admin_service, access_manager);
        server::register_query_routes(app, executor, storage_manager, dbms, mtx, access_manager);
        server_thread = std::thread([this] {
            app.bindaddr("127.0.0.1").port(port).run();
        });
        try {
            wait_until_ready();
        } catch (...) {
            app.stop();
            if (server_thread.joinable()) {
                server_thread.join();
            }
            throw;
        }
    }

    ~RbacHttpFixture() {
        app.stop();
        if (server_thread.joinable()) {
            server_thread.join();
        }
        std::filesystem::remove_all(dir);
    }

    std::string register_user(const std::string &username, const std::string &password) {
        const auto res = http_post(port,
                                   "/register",
                                   "{\"username\":\"" + username + "\",\"password\":\"" + password + "\"}",
                                   "application/json");
        EXPECT_EQ(res.code, crow::status::CREATED) << res.body;
        return token_from(res);
    }

    std::string login(const std::string &username, const std::string &password) {
        const auto res = http_post(port,
                                   "/login",
                                   "{\"username\":\"" + username + "\",\"password\":\"" + password + "\"}",
                                   "application/json");
        EXPECT_EQ(res.code, crow::status::OK) << res.body;
        return token_from(res);
    }

    HttpResponse query(const std::string &token, const std::string &sql) {
        return http_post(port, "/query", sql, "text/plain; charset=utf-8", token);
    }

    HttpResponse grant(const std::string &token,
                       const std::string &subject_type,
                       const std::string &subject_id,
                       const std::string &database_name,
                       const std::string &table_name,
                       const std::string &permission) {
        const auto body = "{\"subject_type\":\"" + subject_type
            + "\",\"subject_id\":\"" + subject_id
            + "\",\"database_name\":\"" + database_name
            + "\",\"table_name\":\"" + table_name
            + "\",\"permission\":\"" + permission + "\"}";
        return http_post(port, "/admin/permissions/grant", body, "application/json", token);
    }

    HttpResponse revoke(const std::string &token,
                        const std::string &subject_type,
                        const std::string &subject_id,
                        const std::string &database_name,
                        const std::string &table_name,
                        const std::string &permission) {
        const auto body = "{\"subject_type\":\"" + subject_type
            + "\",\"subject_id\":\"" + subject_id
            + "\",\"database_name\":\"" + database_name
            + "\",\"table_name\":\"" + table_name
            + "\",\"permission\":\"" + permission + "\"}";
        return http_post(port, "/admin/permissions/revoke", body, "application/json", token);
    }

    void wait_until_ready() const {
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

    std::filesystem::path dir;
    std::uint16_t port;
    DBMS dbms;
    StorageManager storage_manager;
    auth::FileAccountStorage account_storage;
    auth::PasswordHasher hasher;
    auth::JwtService jwt;
    auth::AuthService auth_service;
    rbac::PermissionResolver resolver;
    rbac::AccessManager access_manager;
    services::AdminService admin_service;
    Executor executor;
    std::mutex mtx;
    std::shared_ptr<AccessLogger> logger;
    std::shared_ptr<TelemetryCollector> telemetry;
    TestApp app;
    std::thread server_thread;
};

} // namespace

TEST(RbacHttpIntegration, RegisterAndLoginRoutesIssueJwtForAdminAndRegularUser) {
    RbacHttpFixture fixture("rbac_http_auth_routes_test");

    const auto root_register_token = fixture.register_user("root", "rootpass");
    const auto alice_register_token = fixture.register_user("alice", "alicepass");
    const auto root_login_token = fixture.login("root", "rootpass");
    const auto alice_login_token = fixture.login("alice", "alicepass");

    EXPECT_FALSE(root_register_token.empty());
    EXPECT_FALSE(alice_register_token.empty());
    EXPECT_FALSE(root_login_token.empty());
    EXPECT_FALSE(alice_login_token.empty());

    auth::FileAccountStorage reloaded(fixture.dir);
    EXPECT_TRUE(reloaded.find_by_username("root")->is_admin);
    EXPECT_FALSE(reloaded.find_by_username("alice")->is_admin);
}

TEST(RbacHttpIntegration, AdminCanCreateWriteAndReadThroughQueryRoute) {
    RbacHttpFixture fixture("rbac_http_admin_query_test");
    const auto root_token = fixture.register_user("root", "rootpass");

    const auto create = fixture.query(root_token,
                                      "CREATE DATABASE app;"
                                      "CREATE TABLE app.users (id INT, name STRING);"
                                      "INSERT INTO app.users (id, name) VALUE (1, \"Ann\");");
    EXPECT_EQ(create.code, crow::status::OK) << create.body;

    const auto select = fixture.query(root_token, "SELECT * FROM app.users;");
    EXPECT_EQ(select.code, crow::status::OK) << select.body;
    EXPECT_NE(select.body.find("Ann"), std::string::npos);
}

TEST(RbacHttpIntegration, RegularUserCanReadButCannotWriteByDefaultThroughQueryRoute) {
    RbacHttpFixture fixture("rbac_http_regular_default_permissions_test");
    const auto root_token = fixture.register_user("root", "rootpass");
    const auto alice_token = fixture.register_user("alice", "alicepass");

    const auto setup = fixture.query(root_token,
                                     "CREATE DATABASE app;"
                                     "CREATE TABLE app.users (id INT, name STRING);"
                                     "INSERT INTO app.users (id, name) VALUE (1, \"Ann\");");
    ASSERT_EQ(setup.code, crow::status::OK) << setup.body;

    const auto read = fixture.query(alice_token, "SELECT * FROM app.users;");
    EXPECT_EQ(read.code, crow::status::OK) << read.body;
    EXPECT_NE(read.body.find("Ann"), std::string::npos);

    const auto write = fixture.query(alice_token, "INSERT INTO app.users (id, name) VALUE (2, \"Bob\");");
    EXPECT_EQ(write.code, crow::status::FORBIDDEN);
}

TEST(RbacHttpIntegration, RegularUserCannotUseAdminPermissionRoute) {
    RbacHttpFixture fixture("rbac_http_regular_admin_route_test");
    (void)fixture.register_user("root", "rootpass");
    const auto alice_token = fixture.register_user("alice", "alicepass");
    const auto alice_id = user_id_from_tbl(fixture.dir, "alice");

    const auto res = fixture.grant(alice_token, "user", alice_id, "app", "users", "write_table");

    EXPECT_EQ(res.code, crow::status::FORBIDDEN);
}

TEST(RbacHttpIntegration, AdminCanGrantWriteToRegularUserThroughAdminRoute) {
    RbacHttpFixture fixture("rbac_http_admin_grants_write_test");
    const auto root_token = fixture.register_user("root", "rootpass");
    const auto alice_token = fixture.register_user("alice", "alicepass");
    const auto alice_id = user_id_from_tbl(fixture.dir, "alice");

    const auto setup = fixture.query(root_token, "CREATE DATABASE app;CREATE TABLE app.users (id INT, name STRING);");
    ASSERT_EQ(setup.code, crow::status::OK) << setup.body;

    const auto denied = fixture.query(alice_token, "INSERT INTO app.users (id, name) VALUE (1, \"Ann\");");
    ASSERT_EQ(denied.code, crow::status::FORBIDDEN);

    const auto grant = fixture.grant(root_token, "user", alice_id, "app", "users", "write_table");
    ASSERT_EQ(grant.code, crow::status::OK) << grant.body;

    const auto allowed = fixture.query(alice_token, "INSERT INTO app.users (id, name) VALUE (1, \"Ann\");");
    EXPECT_EQ(allowed.code, crow::status::OK) << allowed.body;
}

TEST(RbacHttpIntegration, AdminCanGrantWildcardWriteToRegularUserThroughAdminRoute) {
    RbacHttpFixture fixture("rbac_http_admin_grants_wildcard_write_test");
    const auto root_token = fixture.register_user("root", "rootpass");
    const auto alice_token = fixture.register_user("alice", "alicepass");
    const auto alice_id = user_id_from_tbl(fixture.dir, "alice");

    const auto setup = fixture.query(root_token,
                                     "CREATE DATABASE app;"
                                     "CREATE TABLE app.users (id INT, name STRING);"
                                     "CREATE DATABASE analytics;"
                                     "CREATE TABLE analytics.events (id INT, name STRING);");
    ASSERT_EQ(setup.code, crow::status::OK) << setup.body;

    const auto grant = fixture.grant(root_token, "user", alice_id, "*", "*", "write_table");
    ASSERT_EQ(grant.code, crow::status::OK) << grant.body;

    EXPECT_EQ(fixture.query(alice_token, "INSERT INTO app.users (id, name) VALUE (1, \"Ann\");").code,
              crow::status::OK);
    EXPECT_EQ(fixture.query(alice_token, "INSERT INTO analytics.events (id, name) VALUE (1, \"Click\");").code,
              crow::status::OK);
}

TEST(RbacHttpIntegration, AdminCanRevokeReadFromRegularUserThroughAdminRoute) {
    RbacHttpFixture fixture("rbac_http_admin_revokes_read_test");
    const auto root_token = fixture.register_user("root", "rootpass");
    const auto alice_token = fixture.register_user("alice", "alicepass");
    const auto alice_id = user_id_from_tbl(fixture.dir, "alice");

    const auto setup = fixture.query(root_token,
                                     "CREATE DATABASE app;"
                                     "CREATE TABLE app.users (id INT, name STRING);"
                                     "INSERT INTO app.users (id, name) VALUE (1, \"Ann\");");
    ASSERT_EQ(setup.code, crow::status::OK) << setup.body;
    ASSERT_EQ(fixture.query(alice_token, "SELECT * FROM app.users;").code, crow::status::OK);

    const auto revoke = fixture.revoke(root_token, "user", alice_id, "app", "users", "read_table");
    ASSERT_EQ(revoke.code, crow::status::OK) << revoke.body;

    EXPECT_EQ(fixture.query(alice_token, "SELECT * FROM app.users;").code, crow::status::FORBIDDEN);
    EXPECT_EQ(fixture.query(root_token, "SELECT * FROM app.users;").code, crow::status::OK);
}

TEST(RbacHttpIntegration, AdminCanRevokePreviouslyGrantedWriteThroughAdminRoute) {
    RbacHttpFixture fixture("rbac_http_admin_revokes_write_test");
    const auto root_token = fixture.register_user("root", "rootpass");
    const auto alice_token = fixture.register_user("alice", "alicepass");
    const auto alice_id = user_id_from_tbl(fixture.dir, "alice");

    const auto setup = fixture.query(root_token, "CREATE DATABASE app;CREATE TABLE app.users (id INT, name STRING);");
    ASSERT_EQ(setup.code, crow::status::OK) << setup.body;

    ASSERT_EQ(fixture.grant(root_token, "user", alice_id, "app", "users", "write_table").code, crow::status::OK);
    ASSERT_EQ(fixture.query(alice_token, "INSERT INTO app.users (id, name) VALUE (1, \"Ann\");").code,
              crow::status::OK);

    const auto revoke = fixture.revoke(root_token, "user", alice_id, "app", "users", "write_table");
    ASSERT_EQ(revoke.code, crow::status::OK) << revoke.body;

    EXPECT_EQ(fixture.query(alice_token, "INSERT INTO app.users (id, name) VALUE (2, \"Bob\");").code,
              crow::status::FORBIDDEN);
}
