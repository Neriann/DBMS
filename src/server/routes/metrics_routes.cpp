#include "server/routes/metrics_routes.hpp"

namespace server {

void register_metrics_routes(crow::App<AccessLogMiddleware, AuthMiddleware> &app,
                             std::shared_ptr<TelemetryCollector> telemetry) {
    CROW_ROUTE(app, "/metrics").methods(crow::HTTPMethod::Get)(
        [telemetry]() {
            crow::response response(telemetry->snapshot_json());
            response.code = 200;
            response.set_header("Content-Type", "application/json");
            return response;
        });
}

} // namespace server
