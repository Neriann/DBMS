#include "server/access_logger.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <string>

namespace {
    std::filesystem::path make_test_dir(const std::string &name) {
        const auto dir = std::filesystem::temp_directory_path() / ("dbms_" + name);
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
        return dir;
    }

    nlohmann::json read_first_log_entry(const std::filesystem::path &path) {
        std::ifstream in(path);
        std::string line;
        std::getline(in, line);
        return nlohmann::json::parse(line);
    }

    crow::request make_request() {
        crow::request req;
        req.method = crow::HTTPMethod::Post;
        req.raw_url = "/query";
        req.url = "/query";
        req.body = "SELECT * FROM users;";
        req.remote_ip_address = "127.0.0.1";
        return req;
    }
}

TEST(AccessLogger, WritesRequestData) {
    const auto dir = make_test_dir("access_logger_header");
    const auto log_path = dir / "access.log";

    auto req = make_request();
    req.headers.emplace("X-Client-ID", "client-42");

    {
        const crow::response res(200, "OK");
        const auto started_at = std::chrono::system_clock::from_time_t(1700000000);
        const auto finished_at = started_at + std::chrono::milliseconds(25);
        AccessLogger logger(log_path);
        logger.log(req, res, started_at, finished_at, "handler-1");
    }

    const auto entry = read_first_log_entry(log_path);

    EXPECT_EQ(entry.at("request_body"), "SELECT * FROM users;");
    EXPECT_EQ(entry.at("client_id"), "client-42");
    EXPECT_EQ(entry.at("handler_id"), "handler-1");
    EXPECT_EQ(entry.at("status_code"), 200);
    EXPECT_EQ(entry.at("method"), "POST");
    EXPECT_EQ(entry.at("url"), "/query");
    const auto started = entry.at("started_at").get<std::string>();
    const auto finished = entry.at("finished_at").get<std::string>();
    EXPECT_EQ(started.size(), 23);
    EXPECT_EQ(finished.size(), 23);
    EXPECT_EQ(started.at(10), 'T');
    EXPECT_EQ(finished.at(10), 'T');
    EXPECT_EQ(started.at(19), '.');
    EXPECT_EQ(finished.at(19), '.');
}

TEST(AccessLogger, FallsBackToRemoteIpWhenClientIdHeaderIsMissing) {
    const auto dir = make_test_dir("access_logger_remote_ip");
    const auto log_path = dir / "access.log";

    auto req = make_request();
    req.remote_ip_address = "10.0.0.15";

    {
        const crow::response res(400, "Bad request");
        const auto started_at = std::chrono::system_clock::from_time_t(1700000000);
        const auto finished_at = started_at + std::chrono::milliseconds(10);
        AccessLogger logger(log_path);
        logger.log(req, res, started_at, finished_at, "handler-2");
    }

    const auto entry = read_first_log_entry(log_path);

    EXPECT_EQ(entry.at("client_id"), "10.0.0.15");
    EXPECT_EQ(entry.at("status_code"), 400);
}
