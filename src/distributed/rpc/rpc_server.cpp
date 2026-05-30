#include "distributed/rpc/rpc_server.hpp"

namespace distributed::rpc {

RpcServer::RpcServer(rbac::AccessManager &access_manager)
    : access_manager_(access_manager) {
}

RpcResponse RpcServer::execute(const RpcRequest &request, const auth::SessionContext &ctx) {
    (void)request;
    (void)ctx;
    return RpcResponse{501, "rpc server local execution is handled by HTTP /query"};
}

RpcResponse RpcServer::heartbeat(const std::string &node_id) {
    (void)node_id;
    return RpcResponse{200, "OK"};
}

} // namespace distributed::rpc
