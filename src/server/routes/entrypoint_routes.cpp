#include "server/routes/entrypoint_routes.hpp"

#include "query/query_runner.hpp"

#include <iterator>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <variant>

namespace server {
namespace {

template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

std::string value_to_shard_key(const Value &value) {
    if (std::holds_alternative<int>(value)) {
        return std::to_string(std::get<int>(value));
    }
    if (std::holds_alternative<InternedString>(value)) {
        return global_string_pool().get(std::get<InternedString>(value).id);
    }
    return {};
}

std::optional<std::string> equality_id_value(const Condition &condition) {
    if (condition.kind != ConditionKind::Simple || !condition.predicate) {
        return std::nullopt;
    }

    const auto &predicate = *condition.predicate;
    if (predicate.op != CmpOp::EQ) {
        return std::nullopt;
    }

    if (predicate.lhs.kind == ExprKind::Column && predicate.lhs.column == "id"
        && predicate.rhs.kind == ExprKind::Literal) {
        return value_to_shard_key(predicate.rhs.literal);
    }
    if (predicate.rhs.kind == ExprKind::Column && predicate.rhs.column == "id"
        && predicate.lhs.kind == ExprKind::Literal) {
        return value_to_shard_key(predicate.lhs.literal);
    }
    return std::nullopt;
}

distributed::rpc::RpcRequest build_rpc_request(const std::string &sql,
                                               const std::string &auth_header) {
    distributed::rpc::RpcRequest request;
    request.sql = sql;
    if (auth_header.starts_with("Bearer ")) {
        request.auth_token = auth_header.substr(7);
    }

    const auto statements = parse_sql(sql);
    if (statements.size() != 1) {
        return request;
    }

    std::visit(
        Overloaded{
            [&](const CreateDatabaseStmt &) {},
            [&](const DropDatabaseStmt &) {},
            [&](const UseStmt &) {},
            [&](const CreateTableStmt &s) {
                request.database_name = s.db_name;
                request.table_name = s.table_name;
            },
            [&](const DropTableStmt &s) {
                request.database_name = s.db_name;
                request.table_name = s.table_name;
            },
            [&](const InsertStmt &s) {
                request.database_name = s.db_name;
                request.table_name = s.table_name;
                const auto id_it = std::ranges::find(s.columns, std::string{"id"});
                if (id_it != s.columns.end() && !s.rows.empty()) {
                    const auto index = static_cast<std::size_t>(std::distance(s.columns.begin(), id_it));
                    if (index < s.rows.front().size()) {
                        request.shard_key = value_to_shard_key(s.rows.front()[index]);
                    }
                }
            },
            [&](const UpdateStmt &s) {
                request.database_name = s.db_name;
                request.table_name = s.table_name;
                if (s.where) {
                    request.shard_key = equality_id_value(*s.where).value_or("");
                }
            },
            [&](const DeleteStmt &s) {
                request.database_name = s.db_name;
                request.table_name = s.table_name;
                if (s.where) {
                    request.shard_key = equality_id_value(*s.where).value_or("");
                }
            },
            [&](const SelectStmt &s) {
                request.database_name = s.db_name;
                request.table_name = s.table_name;
                if (s.where) {
                    request.shard_key = equality_id_value(*s.where).value_or("");
                }
            },
            [&](const RevertStmt &s) {
                request.database_name = s.db_name;
                request.table_name = s.table_name;
            }},
        statements.front());

    return request;
}

} // namespace

void register_entrypoint_routes(crow::SimpleApp &app,
                                cluster::ClusterManager &cluster_manager,
                                distributed::execution::DistributedExecutor &executor,
                                std::mutex &cluster_mtx) {
    CROW_ROUTE(app, "/nodes").methods(crow::HTTPMethod::Get)([&cluster_manager, &cluster_mtx] {
        std::lock_guard lock(cluster_mtx);
        nlohmann::json nodes = nlohmann::json::array();
        for (const auto &node : cluster_manager.topology().nodes()) {
            nodes.push_back({{"id", node.id},
                             {"host", node.endpoint.host},
                             {"port", node.endpoint.port}});
        }
        return crow::response(200, nodes.dump());
    });

    CROW_ROUTE(app, "/nodes").methods(crow::HTTPMethod::Post)(
        [&cluster_manager, &cluster_mtx](const crow::request &req) {
            try {
                const auto body = nlohmann::json::parse(req.body);
                cluster::NodeInfo node;
                node.id = body.at("id").get<std::string>();
                node.endpoint.host = body.at("host").get<std::string>();
                node.endpoint.port = body.at("port").get<std::uint16_t>();
                if (node.id.empty() || node.endpoint.host.empty() || node.endpoint.port == 0) {
                    return crow::response(400, "invalid node");
                }

                std::lock_guard lock(cluster_mtx);
                cluster_manager.add_node(node);
                return crow::response(201, "OK");
            } catch (const std::exception &e) {
                return crow::response(400, e.what());
            }
        });

    CROW_ROUTE(app, "/nodes/<string>").methods(crow::HTTPMethod::Delete)(
        [&cluster_manager, &cluster_mtx](const std::string &node_id) {
            std::lock_guard lock(cluster_mtx);
            cluster_manager.remove_node(node_id);
            return crow::response(200, "OK");
        });

    CROW_ROUTE(app, "/query").methods(crow::HTTPMethod::Post)(
        [&executor, &cluster_mtx](const crow::request &req) {
            try {
                const auto request = build_rpc_request(req.body, req.get_header_value("Authorization"));
                auth::SessionContext ctx;
                ctx.is_authenticated = !request.auth_token.empty();

                std::lock_guard lock(cluster_mtx);
                const auto response = executor.execute(request, ctx);
                return crow::response(response.status, response.body);
            } catch (const std::exception &e) {
                return crow::response(400, e.what());
            }
        });
}

} // namespace server
