#include "cluster/cluster_manager.hpp"
#include "distributed/routing/query_router.hpp"
#include "distributed/routing/shard_resolver.hpp"
#include "distributed/rpc/rpc_types.hpp"

#include <filesystem>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <utility>
#include <unistd.h>

namespace {

std::filesystem::path temp_cluster_dir(const char *name) {
    const auto path = std::filesystem::temp_directory_path()
        / ("dbms_query_router_test_" + std::string(name) + "_" + std::to_string(::getpid()));
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}

distributed::rpc::RpcRequest make_request(std::string shard_key) {
    distributed::rpc::RpcRequest request;
    request.sql = "SELECT * FROM users WHERE id = 42";
    request.database_name = "app";
    request.table_name = "users";
    request.shard_key = std::move(shard_key);
    return request;
}

std::string shard_key_for(cluster::ClusterManager &manager, const cluster::ShardId expected_shard_id) {
    for (std::size_t value = 0; value < 100000; ++value) {
        auto shard_key = std::to_string(value);
        if (manager.compute_shard_id(shard_key) == expected_shard_id) {
            return shard_key;
        }
    }
    throw std::runtime_error("failed to find shard key for test shard");
}

} // namespace

TEST(QueryRouter, RoutesShardKeyRequestToSingleStorageNode) {
    const auto dir = temp_cluster_dir("single_target");
    cluster::ClusterStateStorage storage(dir);
    cluster::ClusterManager manager(storage);
    manager.add_node(cluster::NodeInfo{"node1", {"127.0.0.1", 9001}});
    manager.add_node(cluster::NodeInfo{"node2", {"127.0.0.1", 9002}});

    distributed::routing::ShardResolver resolver(manager);
    distributed::routing::QueryRouter router(resolver);

    const auto request = make_request("42");
    const auto shard_id = manager.compute_shard_id(request.shard_key);
    const auto expected_owner = manager.resolve_owner(request.database_name, request.table_name, shard_id);

    const auto plan = router.plan(request);

    EXPECT_FALSE(plan.scatter);
    ASSERT_EQ(plan.targets.size(), 1U);
    EXPECT_EQ(plan.targets.front().node_id, expected_owner);
    EXPECT_EQ(plan.targets.front().endpoint.port, manager.find_node(expected_owner)->endpoint.port);

    std::filesystem::remove_all(dir);
}

TEST(QueryRouter, RoutesRequestWithoutShardKeyToAllStorageNodes) {
    const auto dir = temp_cluster_dir("scatter");
    cluster::ClusterStateStorage storage(dir);
    cluster::ClusterManager manager(storage);
    manager.add_node(cluster::NodeInfo{"node1", {"127.0.0.1", 9001}});
    manager.add_node(cluster::NodeInfo{"node2", {"127.0.0.1", 9002}});
    manager.add_node(cluster::NodeInfo{"node3", {"127.0.0.1", 9003}});

    distributed::routing::ShardResolver resolver(manager);
    distributed::routing::QueryRouter router(resolver);

    const auto plan = router.plan(make_request(""));

    EXPECT_TRUE(plan.scatter);
    ASSERT_EQ(plan.targets.size(), 3U);
    EXPECT_EQ(plan.targets[0].node_id, "node1");
    EXPECT_EQ(plan.targets[1].node_id, "node2");
    EXPECT_EQ(plan.targets[2].node_id, "node3");

    std::filesystem::remove_all(dir);
}

TEST(QueryRouter, UsesExplicitShardRegistryOwner) {
    const auto dir = temp_cluster_dir("registry_owner");
    cluster::ClusterStateStorage storage(dir);
    storage.save_nodes({cluster::NodeInfo{"node1", {"127.0.0.1", 9001}},
                        cluster::NodeInfo{"node2", {"127.0.0.1", 9002}}});

    constexpr cluster::ShardId shard_id = 7;
    cluster::ShardRegistry registry;
    registry.set_owner("app", "users", shard_id, "node2");
    storage.save_shard_registry(registry);

    cluster::ClusterManager manager(storage);

    distributed::routing::ShardResolver resolver(manager);
    distributed::routing::QueryRouter router(resolver);

    distributed::rpc::RpcRequest request;
    request.sql = "SELECT * FROM users WHERE id = 7";
    request.database_name = "app";
    request.table_name = "users";
    request.shard_key = shard_key_for(manager, shard_id);

    const auto plan = router.plan(request);

    EXPECT_FALSE(plan.scatter);
    ASSERT_EQ(plan.targets.size(), 1U);
    EXPECT_EQ(plan.targets.front().node_id, "node2");
    EXPECT_EQ(plan.targets.front().endpoint.port, 9002);

    std::filesystem::remove_all(dir);
}

TEST(QueryRouter, ThrowsWhenShardOwnerIsNotRegistered) {
    const auto dir = temp_cluster_dir("missing_owner");
    cluster::ClusterStateStorage storage(dir);
    storage.save_nodes({cluster::NodeInfo{"node1", {"127.0.0.1", 9001}}});

    constexpr cluster::ShardId shard_id = 7;
    cluster::ShardRegistry registry;
    registry.set_owner("app", "users", shard_id, "ghost");
    storage.save_shard_registry(registry);

    cluster::ClusterManager manager(storage);

    distributed::routing::ShardResolver resolver(manager);
    distributed::routing::QueryRouter router(resolver);

    distributed::rpc::RpcRequest request;
    request.sql = "SELECT * FROM users WHERE id = 7";
    request.database_name = "app";
    request.table_name = "users";
    request.shard_key = shard_key_for(manager, shard_id);

    EXPECT_THROW(static_cast<void>(router.plan(request)), std::runtime_error);

    std::filesystem::remove_all(dir);
}
