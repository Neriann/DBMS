#include "distributed/execution/result_merger.hpp"

#include <nlohmann/json.hpp>
#include <sstream>

namespace distributed::execution {

rpc::RpcResponse ResultMerger::merge(const std::vector<rpc::RpcResponse> &partial) const {
    if (partial.empty()) {
        return rpc::RpcResponse{503, "no storage nodes available"};
    }

    for (const auto &response : partial) {
        if (response.status < 200 || response.status >= 300) {
            return response;
        }
    }

    bool all_arrays = true;
    nlohmann::json merged = nlohmann::json::array();
    for (const auto &response : partial) {
        try {
            auto parsed = nlohmann::json::parse(response.body);
            if (!parsed.is_array()) {
                all_arrays = false;
                break;
            }
            for (auto &item : parsed) {
                merged.push_back(std::move(item));
            }
        } catch (...) {
            all_arrays = false;
            break;
        }
    }

    if (all_arrays) {
        return rpc::RpcResponse{200, merged.dump()};
    }

    std::ostringstream out;
    for (const auto &response : partial) {
        if (!response.body.empty()) {
            if (out.tellp() > 0) {
                out << '\n';
            }
            out << response.body;
        }
    }
    return rpc::RpcResponse{200, out.str().empty() ? "OK" : out.str()};
}

} // namespace distributed::execution
