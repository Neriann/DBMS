#pragma once

#include "cluster/types.hpp"

#include <chrono>

namespace cluster {

struct NodeState {
    NodeId id;
    bool alive = false;
    std::chrono::steady_clock::time_point last_seen{};
};

} // namespace cluster

