#include "cluster/topology.hpp"

#include "common/not_implemented.hpp"

namespace cluster {

Topology::Topology(ClusterConfig config)
    : config_(config) {
}

void Topology::upsert_node(const NodeInfo &node) {
    common::not_implemented(node);
}

void Topology::update_state(const NodeState &state) {
    common::not_implemented(state);
}

void Topology::remove_node(const NodeId &node_id) {
    common::not_implemented(node_id);
}

const std::vector<NodeInfo> &Topology::nodes() const {
    common::not_implemented();
    return nodes_;
}

const std::vector<NodeState> &Topology::states() const {
    common::not_implemented();
    return states_;
}

const ClusterConfig &Topology::config() const {
    common::not_implemented();
    return config_;
}

} // namespace cluster

