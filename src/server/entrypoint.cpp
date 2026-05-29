#include "server/entrypoint.hpp"

#include "crow.h"
#include "server/heartbeat.hpp"

#include <iostream>
#include <mutex>
#include <vector>

int run_entrypoint() {
    crow::SimpleApp app;

    std::vector<StorageNode> nodes{
        {
            "127.0.0.1",
            "9001"
        }
    };
    std::mutex nodes_mtx;

    CROW_ROUTE(app, "/query")
    .methods(crow::HTTPMethod::Post)
    ([&nodes, &nodes_mtx](const crow::request& req) {
        std::vector<StorageNode> snapshot;

        {
            std::lock_guard lock(nodes_mtx);
            snapshot = nodes;
        }

        if (snapshot.empty()) {
            return crow::response(500, "no storage nodes available");
        }

        try {
            auto response = send_with_failover(snapshot, req.body);

            return crow::response(
                200,
                extract_body(response));
        } catch (const std::exception& e) {
            return crow::response(503, e.what());
        }
    });

    HeartbeatMonitor heartbeat(nodes, nodes_mtx);

    std::cout << "entrypoint listening on port 8080\n";

    app.port(8080)
       .multithreaded()
       .run();

    return 0;
}
