#include "distributed/routing/shard_resolver.hpp"

namespace distributed::routing {

ShardResolver::ShardResolver(cluster::ClusterManager &cluster_manager)
    : cluster_manager_(cluster_manager) {
}

cluster::ShardId ShardResolver::compute_shard_id(const cluster::ShardKey &key) const {
    return cluster_manager_.compute_shard_id(key);
}

cluster::NodeId ShardResolver::resolve_owner(const cluster::DatabaseName &database,
                                             const cluster::TableName &table,
                                             const cluster::ShardId shard_id) const {
    return cluster_manager_.resolve_owner(database, table, shard_id);
}

const cluster::NodeInfo *ShardResolver::find_node(const cluster::NodeId &node_id) const {
    return cluster_manager_.find_node(node_id);
}

const std::vector<cluster::NodeInfo> &ShardResolver::nodes() const {
    return cluster_manager_.topology().nodes();
}

} // namespace distributed::routing
