#include "cluster/cluster_manager.hpp"
#include "cluster/hash_ring.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <map>
#include <string>
#include <unistd.h>

namespace {

std::filesystem::path temp_cluster_dir(const char *name) {
    const auto path = std::filesystem::temp_directory_path()
        / ("dbms_cluster_test_" + std::string(name) + "_" + std::to_string(::getpid()));
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}

} // namespace

TEST(ClusterManager, PersistsAndReloadsDynamicNodes) {
    const auto dir = temp_cluster_dir("nodes");
    cluster::ClusterStateStorage storage(dir);

    {
        cluster::ClusterManager manager(storage);
        manager.add_node(cluster::NodeInfo{"node1", {"127.0.0.1", 9001}});
        manager.add_node(cluster::NodeInfo{"node2", {"127.0.0.1", 9002}});

        ASSERT_EQ(manager.topology().nodes().size(), 2U);
        EXPECT_NE(manager.find_node("node1"), nullptr);
        EXPECT_NE(manager.find_node("node2"), nullptr);
    }

    cluster::ClusterManager reloaded(storage);
    ASSERT_EQ(reloaded.topology().nodes().size(), 2U);
    EXPECT_EQ(reloaded.find_node("node1")->endpoint.port, 9001);
    EXPECT_EQ(reloaded.find_node("node2")->endpoint.port, 9002);

    std::filesystem::remove_all(dir);
}

TEST(ClusterManager, ResolvesShardOwnersAndRebuildsAfterRemoval) {
    const auto dir = temp_cluster_dir("ring");
    cluster::ClusterStateStorage storage(dir);
    cluster::ClusterManager manager(storage);

    manager.add_node(cluster::NodeInfo{"node1", {"127.0.0.1", 9001}});
    manager.add_node(cluster::NodeInfo{"node2", {"127.0.0.1", 9002}});

    const auto shard = manager.compute_shard_id("42");
    const auto owner = manager.resolve_owner("app", "users", shard);
    EXPECT_TRUE(owner == "node1" || owner == "node2");

    manager.remove_node("node1");
    EXPECT_EQ(manager.topology().nodes().size(), 1U);
    EXPECT_EQ(manager.resolve_owner("app", "users", shard), "node2");

    std::filesystem::remove_all(dir);
}

TEST(ClusterManager, MigratesRegisteredShardFilesWhenOwnerNodeIsRemoved) {
    const auto dir = temp_cluster_dir("remove_migrates_registered_shard");
    cluster::ClusterStateStorage storage(dir);

    cluster::ShardRegistry registry;
    registry.set_owner("app", "users", 7, "node1");
    storage.save_shard_registry(registry);

    const auto source = storage.shard_path("node1", "app", "users", 7);
    std::filesystem::create_directories(source);
    {
        std::ofstream marker(source / "rows.tbl");
        marker << "row payload\n";
    }

    cluster::ClusterManager manager(storage);
    manager.add_node(cluster::NodeInfo{"node1", {"127.0.0.1", 9001}});
    manager.add_node(cluster::NodeInfo{"node2", {"127.0.0.1", 9002}});

    manager.remove_node("node1");

    EXPECT_EQ(manager.resolve_owner("app", "users", 7), "node2");
    EXPECT_EQ(storage.load_shard_registry().owner_of("app", "users", 7), "node2");
    EXPECT_FALSE(std::filesystem::exists(source));
    EXPECT_TRUE(std::filesystem::exists(storage.shard_path("node2", "app", "users", 7) / "rows.tbl"));

    std::filesystem::remove_all(dir);
}

TEST(ClusterManager, MigratesRegisteredShardFilesWhenNewNodeBecomesOwner) {
    const auto dir = temp_cluster_dir("add_migrates_registered_shard");
    cluster::ClusterStateStorage storage(dir);

    const cluster::NodeInfo node1{"node1", {"127.0.0.1", 9001}};
    const cluster::NodeInfo node2{"node2", {"127.0.0.1", 9002}};

    cluster::ConsistentHashRing two_node_ring;
    two_node_ring.rebuild({node1, node2}, 128);

    cluster::ShardId shard = 0;
    for (; shard < 128; ++shard) {
        if (two_node_ring.locate(shard) == "node2") {
            break;
        }
    }
    ASSERT_LT(shard, 128u);

    cluster::ShardRegistry registry;
    registry.set_owner("app", "users", shard, "node1");
    storage.save_shard_registry(registry);

    const auto source = storage.shard_path("node1", "app", "users", shard);
    std::filesystem::create_directories(source);
    {
        std::ofstream marker(source / "rows.tbl");
        marker << "row payload\n";
    }

    cluster::ClusterManager manager(storage);
    manager.add_node(node1);
    manager.add_node(node2);

    EXPECT_EQ(manager.resolve_owner("app", "users", shard), "node2");
    EXPECT_EQ(storage.load_shard_registry().owner_of("app", "users", shard), "node2");
    EXPECT_FALSE(std::filesystem::exists(source));
    EXPECT_TRUE(std::filesystem::exists(storage.shard_path("node2", "app", "users", shard) / "rows.tbl"));

    std::filesystem::remove_all(dir);
}

TEST(ClusterManager, ComputesShardIdWithinValidRange) {
    const auto dir = temp_cluster_dir("shard_range");
    cluster::ClusterStateStorage storage(dir);
    cluster::ClusterManager manager(storage);

    constexpr std::size_t sample_count = 10000;
    constexpr std::size_t shard_count = 128;

    for (std::size_t i = 0; i < sample_count; ++i) {
        const auto shard_key = std::to_string(i);

        const auto shard_id = manager.compute_shard_id(shard_key);

        EXPECT_GE(shard_id, 0u);
        EXPECT_LT(shard_id, shard_count);
    }

    std::filesystem::remove_all(dir);
}

TEST(ClusterManager, DistributesShardKeysApproximatelyEvenlyAcrossStorageNodes) {
    const auto dir = temp_cluster_dir("distribution");
    cluster::ClusterStateStorage storage(dir);
    cluster::ClusterManager manager(storage);

    constexpr std::array nodes{
        "node1",
        "node2",
        "node3",
        "node4"
    };

    std::uint16_t port = 9001;
    for (const auto *node_id : nodes) {
        manager.add_node(cluster::NodeInfo{node_id, {"127.0.0.1", port++}});
    }

    std::map<cluster::NodeId, std::size_t> counts;
    constexpr std::size_t key_count = 20000;
    for (std::size_t id = 0; id < key_count; ++id) {
        const auto shard_key = std::to_string(id);
        const auto shard = manager.compute_shard_id(shard_key);
        const auto owner = manager.resolve_owner("app", "users", shard);
        ++counts[owner];
    }

    ASSERT_EQ(counts.size(), nodes.size());

    const auto expected = static_cast<double>(key_count) / static_cast<double>(nodes.size());
    for (const auto &[node_id, count] : counts) {
        SCOPED_TRACE(node_id + " owns " + std::to_string(count) + " shard keys");
        const auto ratio = static_cast<double>(count) / expected;
        EXPECT_GT(ratio, 0.70);
        EXPECT_LT(ratio, 1.30);
    }

    std::filesystem::remove_all(dir);
}
