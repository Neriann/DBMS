#pragma once

#include <string>

namespace distributed::rpc {

struct RpcRequest {
    std::string sql;
    std::string database_name;
    std::string table_name;
    std::string shard_key;
    std::string auth_token;
};

struct RpcResponse {
    int status = 0;
    std::string body;
};

struct RpcError {
    int status = 0;
    std::string message;
};

} // namespace distributed::rpc

