#pragma once

#include "cluster/types.hpp"

namespace cluster {

struct NodeInfo {
    NodeId id;
    NodeEndpoint endpoint;
};

} // namespace cluster

