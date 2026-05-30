#pragma once

#include "cluster/node_info.hpp"
#include "cluster/types.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace cluster {

class ConsistentHashRing {
public:
    void rebuild(const std::vector<NodeInfo> &nodes, std::size_t virtual_nodes);

    [[nodiscard]] NodeId locate(ShardId shard_id) const;

private:
    std::vector<std::pair<std::uint64_t, NodeId>> ring_;
};

} // namespace cluster
