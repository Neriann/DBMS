#include "distributed/routing/query_router.hpp"

#include <stdexcept>

namespace distributed::routing {

QueryRouter::QueryRouter(ShardResolver &resolver)
    : resolver_(resolver) {
}

RoutePlan QueryRouter::plan(const rpc::RpcRequest &request) const {
    RoutePlan plan;
    if (request.shard_key.empty()) {
        plan.scatter = true;
        for (const auto &node : resolver_.nodes()) {
            plan.targets.push_back(RouteTarget{node.id, node.endpoint});
        }
        return plan;
    }

    const auto shard_id = resolver_.compute_shard_id(request.shard_key);
    const auto owner = resolver_.resolve_owner(request.database_name, request.table_name, shard_id);
    const auto *node = resolver_.find_node(owner);
    if (node == nullptr) {
        throw std::runtime_error("shard owner is not registered: " + owner);
    }
    plan.targets.push_back(RouteTarget{node->id, node->endpoint});
    return plan;
}

} // namespace distributed::routing
