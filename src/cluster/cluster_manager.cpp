#include "cluster/cluster_manager.hpp"

#include "common/not_implemented.hpp"

namespace cluster {

ClusterManager::ClusterManager(ClusterStateStorage &storage)
    : storage_(storage), topology_(ClusterConfig{}) {
}

void ClusterManager::refresh() {
    common::not_implemented();
}

void ClusterManager::add_node(const NodeInfo &node) {
    common::not_implemented(node);
}

void ClusterManager::remove_node(const NodeId &node_id) {
    common::not_implemented(node_id);
}

ShardId ClusterManager::compute_shard_id(const ShardKey &key) const {
    common::not_implemented(key);
    return 0;
}

NodeId ClusterManager::resolve_owner(const DatabaseName &database,
                                     const TableName &table,
                                     const ShardId shard_id) const {
    common::not_implemented(database, table, shard_id);
    return {};
}

const Topology &ClusterManager::topology() const {
    common::not_implemented();
    return topology_;
}

const ShardRegistry &ClusterManager::shard_registry() const {
    common::not_implemented();
    return shard_registry_;
}

} // namespace cluster

