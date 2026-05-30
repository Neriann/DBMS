#pragma once

#include "cluster/types.hpp"
#include "distributed/rpc/rpc_types.hpp"

#include <string>

namespace distributed::rpc {

class RpcClient {
public:
    explicit RpcClient(cluster::NodeEndpoint endpoint);

    [[nodiscard]] RpcResponse execute(const RpcRequest &request) const;
    [[nodiscard]] RpcResponse heartbeat(const std::string &node_id) const;

private:
    cluster::NodeEndpoint endpoint_;
};

} // namespace distributed::rpc

