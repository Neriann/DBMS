#pragma once

#include <cstdint>
#include <string>

namespace cluster {

using NodeId = std::string;
using DatabaseName = std::string;
using TableName = std::string;
using ShardKey = std::string;
using ShardId = std::uint32_t;

struct NodeEndpoint {
    std::string host;
    std::uint16_t port = 0;
};

struct ClusterConfig {
    std::uint32_t shard_count = 4096;
    std::uint32_t replication_factor = 1;
};

struct ShardIdentity {
    DatabaseName database;
    TableName table;
    ShardId shard_id = 0;
};

} // namespace cluster
