#include "cluster/hash_ring.hpp"

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <string>

namespace cluster {

void ConsistentHashRing::rebuild(const std::vector<NodeInfo> &nodes, const std::size_t virtual_nodes) {
    ring_.clear();
    ring_.reserve(nodes.size() * virtual_nodes);

    std::hash<std::string> hash;
    for (const auto &node : nodes) {
        for (std::size_t i = 0; i < virtual_nodes; ++i) {
            ring_.emplace_back(hash(node.id + "#" + std::to_string(i)), node.id);
        }
    }

    std::ranges::sort(ring_, {}, &std::pair<std::uint64_t, NodeId>::first);
}

NodeId ConsistentHashRing::locate(const ShardId shard_id) const {
    if (ring_.empty()) {
        throw std::runtime_error("cluster has no storage nodes");
    }

    std::hash<std::string> hash;
    const auto point = static_cast<std::uint64_t>(hash(std::to_string(shard_id)));
    const auto it = std::ranges::lower_bound(ring_, point, {}, &std::pair<std::uint64_t, NodeId>::first);
    return it == ring_.end() ? ring_.front().second : it->second;
}

} // namespace cluster
