#include "cluster/topology.hpp"

#include <algorithm>

namespace cluster {

Topology::Topology(ClusterConfig config)
    : config_(config) {
}

void Topology::upsert_node(const NodeInfo &node) {
    auto it = std::ranges::find(nodes_, node.id, &NodeInfo::id);
    if (it == nodes_.end()) {
        nodes_.push_back(node);
    } else {
        *it = node;
    }

    auto state_it = std::ranges::find(states_, node.id, &NodeState::id);
    if (state_it == states_.end()) {
        states_.push_back(NodeState{node.id, true, {}});
    } else {
        state_it->alive = true;
    }
}

void Topology::update_state(const NodeState &state) {
    auto it = std::ranges::find(states_, state.id, &NodeState::id);
    if (it == states_.end()) {
        states_.push_back(state);
    } else {
        *it = state;
    }
}

void Topology::remove_node(const NodeId &node_id) {
    std::erase_if(nodes_, [&](const NodeInfo &node) {
        return node.id == node_id;
    });
    std::erase_if(states_, [&](const NodeState &state) {
        return state.id == node_id;
    });
}

const std::vector<NodeInfo> &Topology::nodes() const {
    return nodes_;
}

const std::vector<NodeState> &Topology::states() const {
    return states_;
}

const ClusterConfig &Topology::config() const {
    return config_;
}

const NodeInfo *Topology::find_node(const NodeId &node_id) const {
    const auto it = std::ranges::find(nodes_, node_id, &NodeInfo::id);
    return it == nodes_.end() ? nullptr : &*it;
}

} // namespace cluster
