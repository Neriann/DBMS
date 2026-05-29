#pragma once

#include "cluster/shard_registry.hpp"
#include "cluster/topology.hpp"

namespace cluster {

class RebalanceManager {
public:
    void plan(const Topology &topology, const ShardRegistry &registry) const;
};

} // namespace cluster

