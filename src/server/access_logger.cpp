#include "server/access_logger.hpp"

#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <thread>

AccessLogger::AccessLogger(const std::filesystem::path &path) : out_(path, std::ios::app) {
    if (!out_) {
        throw std::runtime_error("cannot open access log file: " + path.string());
    }
}

void AccessLogger::log(const crow::request &req,
                       const crow::response &res,
                       const std::chrono::system_clock::time_point started_at,
                       const std::chrono::system_clock::time_point finished_at,
                       const std::string &handler_id) {
    const auto header_value = req.get_header_value("X-Client-ID");
    const auto client_id = header_value.empty() ? req.remote_ip_address : header_value;

    const nlohmann::json entry{
        {"request_body", req.body},
        {"client_id", client_id},
        {"handler_id", handler_id},
        {"started_at", format_time(started_at)},
        {"finished_at", format_time(finished_at)},
        {"status_code", res.code},
        {"method", crow::method_name(req.method)},
        {"url", req.raw_url}
    };

    std::lock_guard lock(mtx_);
    out_ << entry.dump() << '\n';
    out_.flush();
}

std::string AccessLogger::format_time(const std::chrono::system_clock::time_point time) {
    const auto time_t = std::chrono::system_clock::to_time_t(time);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()) % 1000;

    std::tm tm{};
    localtime_r(&time_t, &tm);

    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S") << '.' << std::setw(3) << std::setfill('0') << ms.count();
    return out.str();
}

// Crow middleware detection requires this exact non-static signature
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void AccessLogMiddleware::before_handle(crow::request &, crow::response &, context &ctx) const {
    ctx.started_at = std::chrono::system_clock::now();
    ctx.telemetry_started_at = TelemetryCollector::clock::now();

    std::ostringstream id;
    id << std::this_thread::get_id();
    ctx.handler_id = id.str();
}

// Crow middleware detection requires this exact non-static signature
// NOLINTNEXTLINE(readability-non-const-parameter)
void AccessLogMiddleware::after_handle(crow::request &req, crow::response &res, context &ctx) const {
    if (telemetry) {
        telemetry->record_request(ctx.telemetry_started_at, TelemetryCollector::clock::now(), res.code);
    }

    if (!logger) return;

    logger->log(req, res, ctx.started_at, std::chrono::system_clock::now(), ctx.handler_id);
}
