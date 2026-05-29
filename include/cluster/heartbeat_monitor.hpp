#pragma once

#include "cluster/types.hpp"

#include <chrono>
#include <vector>

namespace cluster {

class HeartbeatMonitor {
public:
    using clock = std::chrono::steady_clock;

    void mark_seen(const NodeId &node_id, clock::time_point seen_at);

    [[nodiscard]] std::vector<NodeId> expired(clock::time_point now,
                                              std::chrono::milliseconds timeout) const;
};

} // namespace cluster

