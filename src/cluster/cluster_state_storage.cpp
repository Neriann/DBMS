#include "cluster/cluster_state_storage.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>

namespace cluster {
namespace {

using json = nlohmann::json;

std::filesystem::path metadata_dir(const std::filesystem::path &data_dir) {
    return data_dir / "metadata";
}

std::filesystem::path config_path(const std::filesystem::path &data_dir) {
    return metadata_dir(data_dir) / "cluster_state.json";
}

std::filesystem::path nodes_path(const std::filesystem::path &data_dir) {
    return metadata_dir(data_dir) / "nodes.json";
}

std::filesystem::path shard_ownership_path(const std::filesystem::path &data_dir) {
    return metadata_dir(data_dir) / "shard_ownership.json";
}

void ensure_parent(const std::filesystem::path &path) {
    std::filesystem::create_directories(path.parent_path());
}

} // namespace

ClusterStateStorage::ClusterStateStorage(std::filesystem::path data_dir)
    : data_dir_(std::move(data_dir)) {
    std::filesystem::create_directories(metadata_dir(data_dir_));
}

ClusterConfig ClusterStateStorage::load_config() const {
    const auto path = config_path(data_dir_);
    if (!std::filesystem::exists(path)) {
        ClusterConfig config;
        save_config(config);
        return config;
    }

    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("failed to open cluster config: " + path.string());
    }

    json j;
    in >> j;
    ClusterConfig config;
    config.shard_count = j.value("shard_count", config.shard_count);
    config.replication_factor = j.value("replication_factor", config.replication_factor);
    return config;
}

void ClusterStateStorage::save_config(const ClusterConfig &config) const {
    const auto path = config_path(data_dir_);
    ensure_parent(path);
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        throw std::runtime_error("failed to write cluster config: " + path.string());
    }
    out << json{{"shard_count", config.shard_count},
                {"replication_factor", config.replication_factor}}.dump(2)
        << '\n';
}

std::vector<NodeInfo> ClusterStateStorage::load_nodes() const {
    const auto path = nodes_path(data_dir_);
    if (!std::filesystem::exists(path)) {
        return {};
    }

    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("failed to open cluster nodes: " + path.string());
    }

    json j;
    in >> j;
    std::vector<NodeInfo> nodes;
    nodes.reserve(j.size());
    for (const auto &item : j) {
        NodeInfo node;
        node.id = item.at("id").get<std::string>();
        node.endpoint.host = item.at("host").get<std::string>();
        node.endpoint.port = item.at("port").get<std::uint16_t>();
        nodes.push_back(std::move(node));
    }
    return nodes;
}

void ClusterStateStorage::save_nodes(const std::vector<NodeInfo> &nodes) const {
    json j = json::array();
    for (const auto &node : nodes) {
        j.push_back({{"id", node.id},
                     {"host", node.endpoint.host},
                     {"port", node.endpoint.port}});
    }

    const auto path = nodes_path(data_dir_);
    ensure_parent(path);
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        throw std::runtime_error("failed to write cluster nodes: " + path.string());
    }
    out << j.dump(2) << '\n';
}

ShardRegistry ClusterStateStorage::load_shard_registry() const {
    ShardRegistry registry;
    const auto path = shard_ownership_path(data_dir_);
    if (!std::filesystem::exists(path)) {
        return registry;
    }

    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("failed to open shard ownership: " + path.string());
    }

    json j;
    in >> j;
    for (const auto &item : j) {
        registry.set_owner(item.at("database").get<std::string>(),
                           item.at("table").get<std::string>(),
                           item.at("shard_id").get<ShardId>(),
                           item.at("owner_node").get<std::string>());
    }
    return registry;
}

void ClusterStateStorage::save_shard_registry(const ShardRegistry &registry) const {
    json j = json::array();
    for (const auto &[key, owner] : registry.ownership()) {
        std::istringstream parts(key);
        std::string database;
        std::string table;
        std::string shard;
        std::getline(parts, database, '\t');
        std::getline(parts, table, '\t');
        std::getline(parts, shard, '\t');
        j.push_back({{"database", database},
                     {"table", table},
                     {"shard_id", static_cast<ShardId>(std::stoul(shard))},
                     {"owner_node", owner}});
    }

    const auto path = shard_ownership_path(data_dir_);
    ensure_parent(path);
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        throw std::runtime_error("failed to write shard ownership: " + path.string());
    }
    out << j.dump(2) << '\n';
}

} // namespace cluster
