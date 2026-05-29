#include "distributed/rpc/rpc_server.hpp"

#include "common/not_implemented.hpp"

namespace distributed::rpc {

RpcServer::RpcServer(rbac::AccessManager &access_manager)
    : access_manager_(access_manager) {
}

RpcResponse RpcServer::execute(const RpcRequest &request, const auth::SessionContext &ctx) {
    common::not_implemented(request, ctx);
    return {};
}

RpcResponse RpcServer::heartbeat(const std::string &node_id) {
    common::not_implemented(node_id);
    return {};
}

} // namespace distributed::rpc

