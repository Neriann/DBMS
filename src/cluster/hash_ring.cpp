#include "cluster/hash_ring.hpp"

#include "common/not_implemented.hpp"

namespace cluster {

void ConsistentHashRing::rebuild(const std::vector<NodeInfo> &nodes, const std::size_t virtual_nodes) {
    common::not_implemented(nodes, virtual_nodes);
}

NodeId ConsistentHashRing::locate(const ShardId shard_id) const {
    common::not_implemented(shard_id);
    return {};
}

} // namespace cluster

