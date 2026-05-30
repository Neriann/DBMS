#pragma once

#include "async/task_manager.hpp"
#include "core/dbms.hpp"
#include "query/executor.hpp"
#include "rbac/access_manager.hpp"
#include "server/middleware/access_logger.hpp"
#include "server/middleware/auth_middleware.hpp"
#include "storage/storage_manager.hpp"

#include "crow.h"

#include <mutex>

namespace server {

void register_async_routes(crow::App<AccessLogMiddleware, AuthMiddleware> &app,
                           TaskManager &task_manager,
                           rbac::AccessManager &access_manager);

TaskManager make_task_manager(Executor &exec, StorageManager &storage, DBMS &dbms, std::mutex &mtx);

} // namespace server
