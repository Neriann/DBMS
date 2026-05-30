#pragma once

#include "cluster/cluster_manager.hpp"
#include "cluster/types.hpp"

#include <vector>

namespace distributed::routing {

class ShardResolver {
public:
    explicit ShardResolver(cluster::ClusterManager &cluster_manager);

    [[nodiscard]] cluster::ShardId compute_shard_id(const cluster::ShardKey &key) const;
    [[nodiscard]] cluster::NodeId resolve_owner(const cluster::DatabaseName &database,
                                                const cluster::TableName &table,
                                                cluster::ShardId shard_id) const;
    [[nodiscard]] const cluster::NodeInfo *find_node(const cluster::NodeId &node_id) const;
    [[nodiscard]] const std::vector<cluster::NodeInfo> &nodes() const;

private:
    cluster::ClusterManager &cluster_manager_;
};

} // namespace distributed::routing
