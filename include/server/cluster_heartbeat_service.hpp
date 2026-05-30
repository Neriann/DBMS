#pragma once

#include "cluster/cluster_manager.hpp"

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

namespace server {

class ClusterHeartbeatService {
public:
    ClusterHeartbeatService(cluster::ClusterManager &cluster_manager,
                            std::mutex &cluster_mtx,
                            std::chrono::milliseconds interval);
    ~ClusterHeartbeatService();

    ClusterHeartbeatService(const ClusterHeartbeatService &) = delete;
    ClusterHeartbeatService &operator=(const ClusterHeartbeatService &) = delete;

    void start();
    void stop();

private:
    void run();

    cluster::ClusterManager &cluster_manager_;
    std::mutex &cluster_mtx_;
    std::chrono::milliseconds interval_;
    std::atomic_bool running_{false};
    std::thread worker_;
};

} // namespace server
