#include "server/entrypoint.hpp"

#include "cluster/cluster_manager.hpp"
#include "crow.h"
#include "distributed/execution/distributed_executor.hpp"
#include "distributed/execution/result_merger.hpp"
#include "distributed/execution/scatter_gather.hpp"
#include "distributed/routing/query_router.hpp"
#include "distributed/routing/shard_resolver.hpp"
#include "server/cluster_heartbeat_service.hpp"
#include "server/routes/entrypoint_routes.hpp"

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>

namespace {

std::uint16_t parse_port(const char *text) {
    errno = 0;
    char *end = nullptr;
    const auto parsed = std::strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed <= 0 || parsed > 65535) {
        throw std::runtime_error("invalid port: " + std::string(text));
    }
    return static_cast<std::uint16_t>(parsed);
}

} // namespace

int run_entrypoint(const int argc, char **argv) {
    std::uint16_t port = 8080;
    std::filesystem::path data_dir = "./data/entrypoint";

    try {
        if (argc > 1) {
            port = parse_port(argv[1]);
        }
        if (argc > 2) {
            data_dir = argv[2];
        }
        if (argc > 3) {
            throw std::runtime_error("usage: entrypointer [port] [data_dir]");
        }
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }

    cluster::ClusterStateStorage storage(data_dir);
    cluster::ClusterManager cluster_manager(storage);
    distributed::routing::ShardResolver resolver(cluster_manager);
    distributed::routing::QueryRouter router(resolver);
    distributed::execution::ScatterGather scatter_gather;
    distributed::execution::ResultMerger merger;
    distributed::execution::DistributedExecutor executor(router, scatter_gather, merger);

    std::mutex cluster_mtx;
    server::ClusterHeartbeatService heartbeat(cluster_manager, cluster_mtx, std::chrono::seconds(2));
    heartbeat.start();
    crow::SimpleApp app;
    server::register_entrypoint_routes(app, cluster_manager, executor, cluster_mtx);

    std::cout << "entrypoint listening on port " << port << ", data dir: " << data_dir << "\n";
    app.port(port).multithreaded().run();
    heartbeat.stop();
    return 0;
}
