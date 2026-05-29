#include "distributed/routing/query_router.hpp"

#include "common/not_implemented.hpp"

namespace distributed::routing {

QueryRouter::QueryRouter(ShardResolver &resolver)
    : resolver_(resolver) {
}

RoutePlan QueryRouter::plan(const rpc::RpcRequest &request) const {
    common::not_implemented(request);
    return {};
}

} // namespace distributed::routing

