#pragma once

#include "cluster/node_info.hpp"
#include "cluster/node_state.hpp"
#include "cluster/types.hpp"

#include <vector>

namespace cluster {

class Topology {
public:
    explicit Topology(ClusterConfig config);

    void upsert_node(const NodeInfo &node);
    void update_state(const NodeState &state);
    void remove_node(const NodeId &node_id);

    [[nodiscard]] const std::vector<NodeInfo> &nodes() const;
    [[nodiscard]] const std::vector<NodeState> &states() const;
    [[nodiscard]] const ClusterConfig &config() const;

private:
    ClusterConfig config_;
    std::vector<NodeInfo> nodes_;
    std::vector<NodeState> states_;
};

} // namespace cluster
