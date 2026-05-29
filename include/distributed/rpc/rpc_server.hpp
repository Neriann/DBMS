#pragma once

#include "auth/session_context.hpp"
#include "distributed/rpc/rpc_types.hpp"
#include "rbac/access_manager.hpp"

#include <string>

namespace distributed::rpc {

class RpcServer {
public:
    explicit RpcServer(rbac::AccessManager &access_manager);

    [[nodiscard]] RpcResponse execute(const RpcRequest &request, const auth::SessionContext &ctx);
    [[nodiscard]] RpcResponse heartbeat(const std::string &node_id);

private:
    rbac::AccessManager &access_manager_;
};

} // namespace distributed::rpc

