#pragma once

#include "crow.h"
#include "server/middleware/access_logger.hpp"
#include "server/middleware/auth_middleware.hpp"
#include "server/middleware/telemetry.hpp"

#include <memory>

namespace server {

void register_metrics_routes(crow::App<AccessLogMiddleware, AuthMiddleware> &app,
                             std::shared_ptr<TelemetryCollector> telemetry);

} // namespace server
