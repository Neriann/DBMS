#pragma once

#include "core/dbms.hpp"
#include "crow.h"
#include "query/executor.hpp"
#include "rbac/access_manager.hpp"
#include "server/middleware/access_logger.hpp"
#include "server/middleware/auth_middleware.hpp"
#include "storage/storage_manager.hpp"

#include <mutex>

namespace server {

void register_query_routes(crow::App<AccessLogMiddleware, AuthMiddleware> &app,
                           Executor &exec,
                           StorageManager &storage,
                           DBMS &dbms,
                           std::mutex &mtx,
                           rbac::AccessManager &access_manager);

} // namespace server
