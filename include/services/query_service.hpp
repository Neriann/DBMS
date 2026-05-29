#pragma once

#include "auth/session_context.hpp"

#include <string>

class Executor;

namespace distributed::execution {
class DistributedExecutor;
}

namespace services {

class QueryService {
public:
    QueryService(Executor &executor, distributed::execution::DistributedExecutor &distributed_executor);

    [[nodiscard]] std::string execute(const std::string &sql, const auth::SessionContext &ctx);

private:
    Executor &executor_;
    distributed::execution::DistributedExecutor &distributed_executor_;
};

} // namespace services

