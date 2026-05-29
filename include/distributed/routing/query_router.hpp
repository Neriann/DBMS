#pragma once

#include "distributed/routing/route_plan.hpp"
#include "distributed/routing/shard_resolver.hpp"
#include "distributed/rpc/rpc_types.hpp"

namespace distributed::routing {

class QueryRouter {
public:
    explicit QueryRouter(ShardResolver &resolver);

    [[nodiscard]] RoutePlan plan(const rpc::RpcRequest &request) const;

private:
    ShardResolver &resolver_;
};

} // namespace distributed::routing
