#include "distributed/execution/distributed_executor.hpp"

namespace distributed::execution {

DistributedExecutor::DistributedExecutor(routing::QueryRouter &router,
                                         ScatterGather &scatter_gather,
                                         ResultMerger &merger)
    : router_(router), scatter_gather_(scatter_gather), merger_(merger) {
}

rpc::RpcResponse DistributedExecutor::execute(const rpc::RpcRequest &request,
                                              const auth::SessionContext &ctx) {
    (void)ctx;
    const auto plan = router_.plan(request);
    return merger_.merge(scatter_gather_.execute(plan, request));
}

} // namespace distributed::execution
