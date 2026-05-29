#include "cluster/shard_registry.hpp"

#include "common/not_implemented.hpp"

namespace cluster {

void ShardRegistry::set_owner(const DatabaseName &database,
                              const TableName &table,
                              const ShardId shard_id,
                              const NodeId &owner_node) {
    common::not_implemented(database, table, shard_id, owner_node);
}

NodeId ShardRegistry::owner_of(const DatabaseName &database,
                               const TableName &table,
                               const ShardId shard_id) const {
    common::not_implemented(database, table, shard_id);
    return {};
}

} // namespace cluster

