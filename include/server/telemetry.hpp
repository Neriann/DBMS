#pragma once

#include <chrono>
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>

struct TelemetrySnapshot {
    double current_rps = 0.0;
    double average_rps_10m = 0.0;
    double max_rps_10m = 0.0;
    double average_processing_ms_10s = 0.0;
    std::size_t error_count_1m = 0;
    double error_rate_1m = 0.0;
    std::size_t requests_1m = 0;
};

class TelemetryCollector {
public:
    using clock = std::chrono::steady_clock;

    void record_request(clock::time_point started_at, clock::time_point finished_at, int status_code) const;

    TelemetrySnapshot snapshot(clock::time_point now = clock::now()) const;

    std::string snapshot_json(clock::time_point now = clock::now()) const;

private:
    struct RequestMetric {
        clock::time_point finished_at;
        std::chrono::microseconds processing_time{};
        bool error{};
    };

    static bool is_error_status(int status_code);

    void clean_metrics(clock::time_point now) const;

    mutable std::mutex mtx_;
    mutable std::deque<RequestMetric> samples_;
};
