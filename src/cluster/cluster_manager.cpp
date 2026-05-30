#include "cluster/cluster_manager.hpp"

#include <functional>
#include <stdexcept>
#include <string>

namespace cluster {

ClusterManager::ClusterManager(ClusterStateStorage &storage)
    : storage_(storage), topology_(ClusterConfig{}) {
    refresh();
}

void ClusterManager::refresh() {
    const auto config = storage_.load_config();
    topology_ = Topology(config);
    for (const auto &node : storage_.load_nodes()) {
        topology_.upsert_node(node);
    }
    shard_registry_ = storage_.load_shard_registry();
    ring_.rebuild(topology_.nodes(), virtual_nodes_);
}

void ClusterManager::add_node(const NodeInfo &node) {
    topology_.upsert_node(node);
    storage_.save_nodes(topology_.nodes());
    ring_.rebuild(topology_.nodes(), virtual_nodes_);
}

void ClusterManager::remove_node(const NodeId &node_id) {
    topology_.remove_node(node_id);
    storage_.save_nodes(topology_.nodes());
    ring_.rebuild(topology_.nodes(), virtual_nodes_);
}

ShardId ClusterManager::compute_shard_id(const ShardKey &key) const {
    const auto shard_count = topology_.config().shard_count;
    if (shard_count == 0) {
        throw std::runtime_error("cluster shard_count must be greater than zero");
    }
    return static_cast<ShardId>(std::hash<std::string>{}(key) % shard_count);
}

NodeId ClusterManager::resolve_owner(const DatabaseName &database,
                                     const TableName &table,
                                     const ShardId shard_id) const {
    if (const auto owner = shard_registry_.owner_of(database, table, shard_id); !owner.empty()) {
        return owner;
    }
    return ring_.locate(shard_id);
}

const Topology &ClusterManager::topology() const {
    return topology_;
}

const ShardRegistry &ClusterManager::shard_registry() const {
    return shard_registry_;
}

const NodeInfo *ClusterManager::find_node(const NodeId &node_id) const {
    return topology_.find_node(node_id);
}

} // namespace cluster
