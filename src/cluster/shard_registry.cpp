#include "cluster/shard_registry.hpp"

#include <stdexcept>

namespace cluster {

void ShardRegistry::set_owner(const DatabaseName &database,
                              const TableName &table,
                              const ShardId shard_id,
                              const NodeId &owner_node) {
    ownership_[key(database, table, shard_id)] = owner_node;
}

NodeId ShardRegistry::owner_of(const DatabaseName &database,
                               const TableName &table,
                               const ShardId shard_id) const {
    const auto it = ownership_.find(key(database, table, shard_id));
    if (it == ownership_.end()) {
        return {};
    }
    return it->second;
}

const std::map<std::string, NodeId> &ShardRegistry::ownership() const noexcept {
    return ownership_;
}

void ShardRegistry::clear() {
    ownership_.clear();
}

std::string ShardRegistry::key(const DatabaseName &database, const TableName &table, const ShardId shard_id) {
    return database + '\t' + table + '\t' + std::to_string(shard_id);
}

} // namespace cluster
