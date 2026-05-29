#include "distributed/rpc/rpc_client.hpp"

#include "common/not_implemented.hpp"

namespace distributed::rpc {

RpcClient::RpcClient(cluster::NodeEndpoint endpoint)
    : endpoint_(std::move(endpoint)) {
}

RpcResponse RpcClient::execute(const RpcRequest &request) const {
    common::not_implemented(request);
    return {};
}

RpcResponse RpcClient::heartbeat(const std::string &node_id) const {
    common::not_implemented(node_id);
    return {};
}

} // namespace distributed::rpc

