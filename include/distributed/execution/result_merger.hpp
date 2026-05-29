#pragma once

#include "distributed/rpc/rpc_types.hpp"

#include <vector>

namespace distributed::execution {

class ResultMerger {
public:
    [[nodiscard]] rpc::RpcResponse merge(const std::vector<rpc::RpcResponse> &partial) const;
};

} // namespace distributed::execution

