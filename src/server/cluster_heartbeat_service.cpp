#include "server/cluster_heartbeat_service.hpp"

#include "distributed/rpc/rpc_client.hpp"

#include <iostream>
#include <vector>

namespace server {

ClusterHeartbeatService::ClusterHeartbeatService(cluster::ClusterManager &cluster_manager,
                                                 std::mutex &cluster_mtx,
                                                 const std::chrono::milliseconds interval)
    : cluster_manager_(cluster_manager), cluster_mtx_(cluster_mtx), interval_(interval) {
}

ClusterHeartbeatService::~ClusterHeartbeatService() {
    stop();
}

void ClusterHeartbeatService::start() {
    if (running_.exchange(true)) {
        return;
    }
    worker_ = std::thread(&ClusterHeartbeatService::run, this);
}

void ClusterHeartbeatService::stop() {
    running_ = false;
    if (worker_.joinable()) {
        worker_.join();
    }
}

void ClusterHeartbeatService::run() {
    while (running_.load()) {
        std::this_thread::sleep_for(interval_);

        std::vector<cluster::NodeInfo> nodes;
        {
            std::lock_guard lock(cluster_mtx_);
            nodes = cluster_manager_.topology().nodes();
        }

        for (const auto &node : nodes) {
            try {
                distributed::rpc::RpcClient client(node.endpoint);
                const auto response = client.heartbeat(node.id);
                if (response.status == 200) {
                    continue;
                }
            } catch (const std::exception &) {
            }

            std::lock_guard lock(cluster_mtx_);
            if (cluster_manager_.find_node(node.id) != nullptr) {
                cluster_manager_.remove_node(node.id);
                std::cerr << "storage node expired: " << node.id << "\n";
            }
        }
    }
}

} // namespace server
