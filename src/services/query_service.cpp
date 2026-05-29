#include "services/query_service.hpp"

#include "common/not_implemented.hpp"

namespace services {

QueryService::QueryService(Executor &executor, distributed::execution::DistributedExecutor &distributed_executor)
    : executor_(executor), distributed_executor_(distributed_executor) {
}

std::string QueryService::execute(const std::string &sql, const auth::SessionContext &ctx) {
    common::not_implemented(sql, ctx);
    return {};
}

} // namespace services

