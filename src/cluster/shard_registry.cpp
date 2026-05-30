#include "cluster/shard_registry.hpp"

#include <sstream>
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

std::vector<ShardPlacement> ShardRegistry::placements() const {
    std::vector<ShardPlacement> result;
    result.reserve(ownership_.size());
    for (const auto &[stored_key, owner] : ownership_) {
        result.push_back(placement_from_key(stored_key, owner));
    }
    return result;
}

const std::map<std::string, NodeId> &ShardRegistry::ownership() const noexcept {
    return ownership_;
}

void ShardRegistry::clear() {
    ownership_.clear();
}

ShardPlacement ShardRegistry::placement_from_key(const std::string &key, const NodeId &owner_node) {
    std::istringstream parts(key);
    std::string database;
    std::string table;
    std::string shard;
    std::getline(parts, database, '\t');
    std::getline(parts, table, '\t');
    std::getline(parts, shard, '\t');
    if (database.empty() || table.empty() || shard.empty()) {
        throw std::runtime_error("invalid shard registry key");
    }
    return ShardPlacement{database, table, static_cast<ShardId>(std::stoul(shard)), owner_node};
}

std::string ShardRegistry::key(const DatabaseName &database, const TableName &table, const ShardId shard_id) {
    return database + '\t' + table + '\t' + std::to_string(shard_id);
}

} // namespace cluster
