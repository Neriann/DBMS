#pragma once

#include "cluster/cluster_manager.hpp"
#include "cluster/types.hpp"

namespace distributed::routing {

class ShardResolver {
public:
    explicit ShardResolver(cluster::ClusterManager &cluster_manager);

    [[nodiscard]] cluster::ShardId compute_shard_id(const cluster::ShardKey &key) const;
    [[nodiscard]] cluster::NodeId resolve_owner(const cluster::DatabaseName &database,
                                                const cluster::TableName &table,
                                                cluster::ShardId shard_id) const;

private:
    cluster::ClusterManager &cluster_manager_;
};

} // namespace distributed::routing

