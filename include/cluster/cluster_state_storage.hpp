#pragma once

#include "cluster/node_info.hpp"
#include "cluster/shard_registry.hpp"
#include "cluster/types.hpp"

#include <filesystem>
#include <vector>

namespace cluster {

class ClusterStateStorage {
public:
    explicit ClusterStateStorage(std::filesystem::path data_dir);

    [[nodiscard]] ClusterConfig load_config() const;
    void save_config(const ClusterConfig &config) const;

    [[nodiscard]] std::vector<NodeInfo> load_nodes() const;
    void save_nodes(const std::vector<NodeInfo> &nodes) const;

    [[nodiscard]] ShardRegistry load_shard_registry() const;
    void save_shard_registry(const ShardRegistry &registry) const;
    void migrate_shard_files(const ShardPlacement &from, const NodeId &to_node) const;

    [[nodiscard]] std::filesystem::path shard_path(const NodeId &node_id,
                                                   const DatabaseName &database,
                                                   const TableName &table,
                                                   ShardId shard_id) const;

private:
    std::filesystem::path data_dir_;
};

} // namespace cluster
