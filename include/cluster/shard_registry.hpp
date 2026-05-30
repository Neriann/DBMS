#pragma once

#include "cluster/types.hpp"

#include <map>
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

    [[nodiscard]] const std::map<std::string, NodeId> &ownership() const noexcept;

    void clear();

private:
    static std::string key(const DatabaseName &database, const TableName &table, ShardId shard_id);

    std::map<std::string, NodeId> ownership_;
};

} // namespace cluster
