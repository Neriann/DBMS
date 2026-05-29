#include "distributed/execution/distributed_executor.hpp"

#include "common/not_implemented.hpp"

namespace distributed::execution {

DistributedExecutor::DistributedExecutor(routing::QueryRouter &router,
                                         ScatterGather &scatter_gather,
                                         ResultMerger &merger)
    : router_(router), scatter_gather_(scatter_gather), merger_(merger) {
}

rpc::RpcResponse DistributedExecutor::execute(const rpc::RpcRequest &request,
                                              const auth::SessionContext &ctx) {
    common::not_implemented(request, ctx);
    return {};
}

} // namespace distributed::execution

