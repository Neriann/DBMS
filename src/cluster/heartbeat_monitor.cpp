#include "cluster/heartbeat_monitor.hpp"

namespace cluster {

void HeartbeatMonitor::mark_seen(const NodeId &node_id, const clock::time_point seen_at) {
    seen_[node_id] = seen_at;
}

std::vector<NodeId> HeartbeatMonitor::expired(const clock::time_point now,
                                              const std::chrono::milliseconds timeout) const {
    std::vector<NodeId> result;
    for (const auto &[node_id, seen_at] : seen_) {
        if (now - seen_at > timeout) {
            result.push_back(node_id);
        }
    }
    return result;
}

} // namespace cluster
