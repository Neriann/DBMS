#include "cluster/heartbeat_monitor.hpp"

#include "common/not_implemented.hpp"

namespace cluster {

void HeartbeatMonitor::mark_seen(const NodeId &node_id, const clock::time_point seen_at) {
    common::not_implemented(node_id, seen_at);
}

std::vector<NodeId> HeartbeatMonitor::expired(const clock::time_point now,
                                              const std::chrono::milliseconds timeout) const {
    common::not_implemented(now, timeout);
    return {};
}

} // namespace cluster

