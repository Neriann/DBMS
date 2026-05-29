#include "cluster/cluster_state_storage.hpp"

#include "common/not_implemented.hpp"

namespace cluster {

ClusterStateStorage::ClusterStateStorage(std::filesystem::path data_dir)
    : data_dir_(std::move(data_dir)) {
}

ClusterConfig ClusterStateStorage::load_config() const {
    common::not_implemented();
    return {};
}

void ClusterStateStorage::save_config(const ClusterConfig &config) const {
    common::not_implemented(config);
}

std::vector<NodeInfo> ClusterStateStorage::load_nodes() const {
    common::not_implemented();
    return {};
}

void ClusterStateStorage::save_nodes(const std::vector<NodeInfo> &nodes) const {
    common::not_implemented(nodes);
}

ShardRegistry ClusterStateStorage::load_shard_registry() const {
    common::not_implemented();
    return {};
}

void ClusterStateStorage::save_shard_registry(const ShardRegistry &registry) const {
    common::not_implemented(registry);
}

} // namespace cluster

