#pragma once

#include "crow.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>

class AccessLogger {
public:
    explicit AccessLogger(const std::filesystem::path &path);

    void log(const crow::request &req,
             const crow::response &res,
             std::chrono::system_clock::time_point started_at,
             std::chrono::system_clock::time_point finished_at,
             const std::string &handler_id);

private:
    static std::string format_time(std::chrono::system_clock::time_point time);

    std::mutex mtx_;
    std::ofstream out_;
};

struct AccessLogMiddleware {
    struct context {
        std::chrono::system_clock::time_point started_at;
        std::string handler_id;
    };

    std::shared_ptr<AccessLogger> logger;

    void before_handle(crow::request &req, crow::response &res, context &ctx) const;

    void after_handle(crow::request &req, crow::response &res, context &ctx) const;
};
