#pragma once

#include "distributed/routing/route_plan.hpp"
#include "distributed/rpc/rpc_client.hpp"
#include "distributed/rpc/rpc_types.hpp"

#include <vector>

namespace distributed::execution {

class ScatterGather {
public:
    [[nodiscard]] std::vector<rpc::RpcResponse> execute(const routing::RoutePlan &plan,
                                                       const rpc::RpcRequest &request) const;
};

} // namespace distributed::execution

