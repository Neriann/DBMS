#pragma once

#include "cluster/cluster_state_storage.hpp"
#include "cluster/hash_ring.hpp"
#include "cluster/heartbeat_monitor.hpp"
#include "cluster/rebalance_manager.hpp"
#include "cluster/shard_registry.hpp"
#include "cluster/topology.hpp"
#include "cluster/types.hpp"

#include <cstddef>

namespace cluster {

class ClusterManager {
public:
    explicit ClusterManager(ClusterStateStorage &storage);

    void refresh();
    void add_node(const NodeInfo &node);
    void remove_node(const NodeId &node_id);

    [[nodiscard]] ShardId compute_shard_id(const ShardKey &key) const;
    [[nodiscard]] NodeId resolve_owner(const DatabaseName &database,
                                       const TableName &table,
                                       ShardId shard_id) const;

    [[nodiscard]] const Topology &topology() const;
    [[nodiscard]] const ShardRegistry &shard_registry() const;

private:
    ClusterStateStorage &storage_;
    Topology topology_;
    ShardRegistry shard_registry_;
    ConsistentHashRing ring_;
    HeartbeatMonitor heartbeat_;
    RebalanceManager rebalance_;
    std::size_t virtual_nodes_ = 128;
};

} // namespace cluster
