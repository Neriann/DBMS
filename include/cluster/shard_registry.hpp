#pragma once

#include "cluster/types.hpp"

#include <string>

namespace cluster {

class ShardRegistry {
public:
    void set_owner(const DatabaseName &database,
                   const TableName &table,
                   ShardId shard_id,
                   const NodeId &owner_node);

    [[nodiscard]] NodeId owner_of(const DatabaseName &database,
                                  const TableName &table,
                                  ShardId shard_id) const;
};

} // namespace cluster

