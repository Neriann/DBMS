#include "distributed/execution/scatter_gather.hpp"

#include "common/not_implemented.hpp"

namespace distributed::execution {

std::vector<rpc::RpcResponse> ScatterGather::execute(const routing::RoutePlan &plan,
                                                     const rpc::RpcRequest &request) const {
    common::not_implemented(plan, request);
    return {};
}

} // namespace distributed::execution

