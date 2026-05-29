#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct StorageNode {
    std::string host;
    std::string port;
    bool alive = true;
};

class HeartbeatMonitor {
public:
    explicit HeartbeatMonitor(
        std::vector<StorageNode>& nodes,
        std::mutex& nodes_mtx,
        std::chrono::milliseconds interval = std::chrono::seconds(2));

    HeartbeatMonitor(const HeartbeatMonitor&) = delete;
    HeartbeatMonitor& operator=(const HeartbeatMonitor&) = delete;

    ~HeartbeatMonitor();

private:
    void loop();

    std::vector<StorageNode>* nodes_;
    std::mutex* nodes_mtx_;
    std::chrono::milliseconds interval_;
    std::atomic<bool> running_{true};
    std::thread worker_;
};

bool ping_node(const StorageNode& node);

void restart_node(const StorageNode& node);

std::string send_with_failover(
    std::vector<StorageNode>& nodes,
    const std::string& request);

std::string extract_body(const std::string& response);
