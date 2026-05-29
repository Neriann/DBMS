#pragma once

#include "auth/session_context.hpp"
#include "distributed/execution/result_merger.hpp"
#include "distributed/execution/scatter_gather.hpp"
#include "distributed/routing/query_router.hpp"
#include "distributed/rpc/rpc_types.hpp"

namespace distributed::execution {

class DistributedExecutor {
public:
    DistributedExecutor(routing::QueryRouter &router, ScatterGather &scatter_gather, ResultMerger &merger);

    [[nodiscard]] rpc::RpcResponse execute(const rpc::RpcRequest &request, const auth::SessionContext &ctx);

private:
    routing::QueryRouter &router_;
    ScatterGather &scatter_gather_;
    ResultMerger &merger_;
};

} // namespace distributed::execution

