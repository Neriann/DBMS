#include "cluster/rebalance_manager.hpp"

#include "common/not_implemented.hpp"

namespace cluster {

void RebalanceManager::plan(const Topology &topology, const ShardRegistry &registry) const {
    common::not_implemented(topology, registry);
}

} // namespace cluster

