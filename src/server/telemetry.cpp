#include "server/telemetry.hpp"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <unordered_map>

void TelemetryCollector::record_request(const clock::time_point started_at, const clock::time_point finished_at,
                                        const int status_code) const {
    auto processing_time = std::chrono::duration_cast<std::chrono::microseconds>(finished_at - started_at);
    if (processing_time < std::chrono::microseconds::zero()) processing_time = std::chrono::microseconds::zero();

    std::lock_guard lock(mtx_);
    samples_.push_back(RequestMetric{finished_at, processing_time, is_error_status(status_code)});
    clean_metrics(finished_at);
}

TelemetrySnapshot TelemetryCollector::snapshot(const clock::time_point now) const {
    std::lock_guard lock(mtx_);
    clean_metrics(now);

    constexpr auto one_second = std::chrono::seconds(1);
    constexpr auto ten_seconds = std::chrono::seconds(10);
    constexpr auto one_minute = std::chrono::minutes(1);
    constexpr auto ten_minutes = std::chrono::minutes(10);

    const auto current_rps_start = now - one_second;
    const auto processing_start = now - ten_seconds;
    const auto errors_start = now - one_minute;
    const auto rps_start = now - ten_minutes;

    TelemetrySnapshot result{};
    std::size_t processing_count = 0;
    std::chrono::microseconds processing_sum{0};
    std::unordered_map<long long, std::size_t> requests_by_second;

    for (const auto &[finished_at, processing_time, error]: samples_) {
        if (finished_at < rps_start || finished_at > now) continue;

        ++result.average_rps_10m;
        const auto second = std::chrono::duration_cast<std::chrono::seconds>(
            finished_at.time_since_epoch()).count();
        ++requests_by_second[second];

        if (finished_at >= current_rps_start) {
            ++result.current_rps;
        }
        if (finished_at >= processing_start) {
            processing_sum += processing_time;
            ++processing_count;
        }
        if (finished_at >= errors_start) {
            ++result.requests_1m;
            if (error) {
                ++result.error_count_1m;
            }
        }
    }

    result.average_rps_10m /= std::chrono::duration<double>(ten_minutes).count();
    for (const auto &requests: requests_by_second | std::views::values) {
        result.max_rps_10m = std::max(result.max_rps_10m, static_cast<double>(requests));
    }
    if (processing_count != 0) {
        result.average_processing_ms_10s = std::chrono::duration<double, std::milli>(processing_sum).count() /
                                           static_cast<double>(processing_count);
    }
    if (result.requests_1m != 0) {
        result.error_rate_1m = static_cast<double>(result.error_count_1m) / static_cast<double>(result.requests_1m);
    }

    return result;
}

std::string TelemetryCollector::snapshot_json(const clock::time_point now) const {
    const auto [current_rps
        ,average_rps_10m
        ,max_rps_10m
        ,average_processing_ms_10s
        ,error_count_1m
        ,error_rate_1m
        ,requests_1m
    ] = snapshot(now);
    const nlohmann::json body{
        {"current_rps", current_rps},
        {"average_rps_10m", average_rps_10m},
        {"max_rps_10m", max_rps_10m},
        {"average_processing_ms_10s", average_processing_ms_10s},
        {"error_count_1m", error_count_1m},
        {"error_rate_1m", error_rate_1m},
        {"requests_1m", requests_1m}
    };
    return body.dump();
}

bool TelemetryCollector::is_error_status(const int status_code) {
    return status_code >= 400;
}

void TelemetryCollector::clean_metrics(const clock::time_point now) const {
    const auto cutoff = now - std::chrono::minutes(10);
    while (!samples_.empty() && samples_.front().finished_at < cutoff) {
        samples_.pop_front();
    }
}
