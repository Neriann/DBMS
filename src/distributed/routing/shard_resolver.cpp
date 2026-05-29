#include "distributed/routing/shard_resolver.hpp"

#include "common/not_implemented.hpp"

namespace distributed::routing {

ShardResolver::ShardResolver(cluster::ClusterManager &cluster_manager)
    : cluster_manager_(cluster_manager) {
}

cluster::ShardId ShardResolver::compute_shard_id(const cluster::ShardKey &key) const {
    common::not_implemented(key);
    return 0;
}

cluster::NodeId ShardResolver::resolve_owner(const cluster::DatabaseName &database,
                                             const cluster::TableName &table,
                                             const cluster::ShardId shard_id) const {
    common::not_implemented(database, table, shard_id);
    return {};
}

} // namespace distributed::routing

