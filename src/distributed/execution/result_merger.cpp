#include "distributed/execution/result_merger.hpp"

#include "common/not_implemented.hpp"

namespace distributed::execution {

rpc::RpcResponse ResultMerger::merge(const std::vector<rpc::RpcResponse> &partial) const {
    common::not_implemented(partial);
    return {};
}

} // namespace distributed::execution

