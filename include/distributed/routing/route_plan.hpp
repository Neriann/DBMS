#pragma once

#include "cluster/types.hpp"

#include <vector>

namespace distributed::routing {

struct RouteTarget {
    cluster::NodeId node_id;
    cluster::NodeEndpoint endpoint;
};

struct RoutePlan {
    bool scatter = false;
    std::vector<RouteTarget> targets;
};

} // namespace distributed::routing

