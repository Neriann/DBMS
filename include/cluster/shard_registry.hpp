#pragma once

#include "cluster/types.hpp"

#include <map>
#include <string>
#include <vector>

namespace cluster {

struct ShardPlacement {
    DatabaseName database;
    TableName table;
    ShardId shard_id = 0;
    NodeId owner_node;
};

class ShardRegistry {
public:
    void set_owner(const DatabaseName &database,
                   const TableName &table,
                   ShardId shard_id,
                   const NodeId &owner_node);

    [[nodiscard]] NodeId owner_of(const DatabaseName &database,
                                  const TableName &table,
                                  ShardId shard_id) const;

    [[nodiscard]] std::vector<ShardPlacement> placements() const;
    [[nodiscard]] const std::map<std::string, NodeId> &ownership() const noexcept;

    void clear();

private:
    static ShardPlacement placement_from_key(const std::string &key, const NodeId &owner_node);
    static std::string key(const DatabaseName &database, const TableName &table, ShardId shard_id);

    std::map<std::string, NodeId> ownership_;
};

} // namespace cluster
