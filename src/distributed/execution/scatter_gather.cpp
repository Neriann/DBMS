#include "distributed/execution/scatter_gather.hpp"

namespace distributed::execution {

std::vector<rpc::RpcResponse> ScatterGather::execute(const routing::RoutePlan &plan,
                                                     const rpc::RpcRequest &request) const {
    std::vector<rpc::RpcResponse> responses;
    responses.reserve(plan.targets.size());
    for (const auto &target : plan.targets) {
        rpc::RpcClient client(target.endpoint);
        responses.push_back(client.execute(request));
    }
    return responses;
}

} // namespace distributed::execution
