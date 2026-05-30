#pragma once

#include "cluster/cluster_manager.hpp"
#include "distributed/execution/distributed_executor.hpp"

#include "crow.h"

#include <mutex>

namespace server {

void register_entrypoint_routes(crow::SimpleApp &app,
                                cluster::ClusterManager &cluster_manager,
                                distributed::execution::DistributedExecutor &executor,
                                std::mutex &cluster_mtx);

} // namespace server
