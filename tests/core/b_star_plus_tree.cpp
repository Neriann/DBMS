#include "gtest/gtest.h"

#include <b_star_plus_tree.hpp>
#include <storage/pager.hpp>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <map>
#include <random>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
    std::filesystem::path make_tree_path() {
        const auto name = "dbms_bsp_tree_" +
                          std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                          ".bin";
        return std::filesystem::temp_directory_path() / name;
    }
}

template<typename TKey, typename TValue>
bool compare_results(
    const std::vector<typename BSP_tree<TKey, TValue>::value_type> &expected,
    const std::vector<typename BSP_tree<TKey, TValue>::value_type> &actual) {
    if (expected.size() != actual.size()) return false;

    for (size_t i = 0; i < expected.size(); ++i) {
        if (expected[i].first != actual[i].first) return false;
        if (expected[i].second != actual[i].second) return false;
    }

    return true;
}

template<typename TValue>
bool compare_obtain_results(const std::vector<TValue> &expected, const std::vector<TValue> &actual) {
    if (expected.size() != actual.size()) return false;

    for (size_t i = 0; i < expected.size(); ++i) {
        if (expected[i] != actual[i]) return false;
    }

    return true;
}

template<typename TKey, typename TValue>
struct test_data {
    TKey key;
    TValue value;
    size_t index;

    test_data(const size_t i, TKey k, TValue v) : key(std::move(k)), value(std::move(v)), index(i) {
    }
};

template<typename TKey, typename TValue, typename Comp, size_t t>
bool infix_const_iterator_test(
    const BSP_tree<TKey, TValue, Comp, t> &tree,
    const std::vector<test_data<TKey, TValue> > &expected_result) {
    auto it = tree.begin();

    for (const auto &item: expected_result) {
        if (it == tree.end()) return false;
        if (it->first != item.key || it->second != item.value) return false;
        ++it;
    }

    return it == tree.end();
}

template<typename TKey, typename TValue, typename Comp, size_t t>
void expect_tree_equals(
    const BSP_tree<TKey, TValue, Comp, t> &tree,
    const std::map<TKey, TValue, Comp> &expected) {
    ASSERT_EQ(tree.size(), expected.size());
    tree.validate();

    auto actual_it = tree.begin();
    for (const auto &[key, value]: expected) {
        ASSERT_NE(actual_it, tree.end());
        EXPECT_EQ(actual_it->first, key);
        EXPECT_EQ(actual_it->second, value);
        EXPECT_TRUE(tree.contains(key));
        EXPECT_EQ(tree.at(key), value);
        ++actual_it;
    }

    EXPECT_EQ(actual_it, tree.end());
}

TEST(BTreePositiveTests, EmptyTreeHasEmptyInfixOrder) {
    const auto path = make_tree_path();

    {
        const BSP_tree<int, std::string, std::less<int>, 1024> tree(path);
        EXPECT_TRUE(infix_const_iterator_test(tree, std::vector<test_data<int, std::string>>{}));
    }

    std::filesystem::remove(path);
}

TEST(BTreePositiveTests, SmallOrderTwoInfixOrder) {
    const auto path = make_tree_path();

    {
        const std::vector<test_data<int, std::string> > expected_result = {
            {0, -10, "g"},
            {1, 0, "h"},
            {2, 1, "a"},
            {3, 2, "b"},
            {0, 3, "d"},
            {1, 4, "e"},
            {2, 15, "c"},
            {3, 27, "f"},
        };

        BSP_tree<int, std::string, std::less<>, 2> tree(path);
        tree.emplace(1, "a");
        tree.emplace(2, "b");
        tree.emplace(15, "c");
        tree.emplace(3, "d");
        tree.emplace(4, "e");
        tree.emplace(27, "f");
        tree.emplace(-10, "g");
        tree.emplace(0, "h");

        EXPECT_TRUE(infix_const_iterator_test(tree, expected_result));
    }

    std::filesystem::remove(path);
}

TEST(BTreePositiveTests, MediumOrderThreeInfixOrder) {
    const auto path = make_tree_path();

    {
        const std::vector<test_data<int, std::string> > expected_result = {
            {0, 1, "a"},
            {1, 2, "b"},
            {2, 3, "d"},
            {3, 4, "e"},
            {4, 15, "c"},
            {5, 24, "g"},
            {0, 45, "k"},
            {1, 100, "f"},
            {2, 101, "j"},
            {3, 193, "l"},
            {4, 456, "h"},
            {5, 534, "m"},
        };

        BSP_tree<int, std::string, std::less<>, 3> tree(path);
        for (const auto &[key, value]: std::vector<std::pair<int, std::string> >{
                 {1, "a"}, {2, "b"}, {15, "c"}, {3, "d"}, {4, "e"}, {100, "f"},
                 {24, "g"}, {456, "h"}, {101, "j"}, {45, "k"}, {193, "l"}, {534, "m"}
             }) {
            tree.emplace(key, value);
        }

        EXPECT_TRUE(infix_const_iterator_test(tree, expected_result));
    }

    std::filesystem::remove(path);
}

TEST(BTreePositiveTests, BiggerOrdersKeepSortedInfixOrder) {
    const std::vector<test_data<int, std::string> > expected_result = {
        {0, 1, "a"}, {1, 2, "b"}, {2, 3, "d"}, {3, 4, "e"},
        {4, 15, "c"}, {5, 24, "g"}, {6, 45, "k"}, {7, 100, "f"},
        {8, 101, "j"}, {9, 193, "l"}, {10, 456, "h"}, {11, 534, "m"},
    };
    const std::vector<std::pair<int, std::string> > input = {
        {1, "a"}, {2, "b"}, {15, "c"}, {3, "d"}, {4, "e"}, {100, "f"},
        {24, "g"}, {456, "h"}, {101, "j"}, {45, "k"}, {193, "l"}, {534, "m"}
    };

    {
        const auto path = make_tree_path();
        BSP_tree<int, std::string, std::less<>, 7> tree(path);
        for (const auto &[key, value]: input) tree.emplace(key, value);
        EXPECT_TRUE(infix_const_iterator_test(tree, expected_result));
        std::filesystem::remove(path);
    }

    {
        const auto path = make_tree_path();
        BSP_tree<int, std::string, std::less<> > tree(path);
        for (const auto &[key, value]: input) tree.emplace(key, value);
        EXPECT_TRUE(infix_const_iterator_test(tree, expected_result));
        std::filesystem::remove(path);
    }
}

TEST(BTreePositiveTests, EraseKeepsRemainingValues) {
    const auto path = make_tree_path();

    {
        const std::vector<test_data<int, std::string> > expected_result = {
            {0, 1, "a"},
            {1, 3, "d"},
            {2, 15, "c"},
        };

        BSP_tree<int, std::string, std::less<>, 2> tree(path);
        tree.emplace(1, "a");
        tree.emplace(2, "b");
        tree.emplace(15, "c");
        tree.emplace(3, "d");
        tree.emplace(4, "e");

        EXPECT_EQ(tree.at(2), "b");
        EXPECT_EQ(tree.at(4), "e");

        tree.erase(2);
        tree.erase(4);

        EXPECT_TRUE(infix_const_iterator_test(tree, expected_result));
    }

    std::filesystem::remove(path);
}

TEST(BTreePositiveTests, EraseCanMergeTerms) {
    const auto path = make_tree_path();

    {
        const std::vector<test_data<int, std::string> > expected_before_erase = {
            {0, 1, "a"}, {1, 2, "b"}, {2, 3, "d"}, {3, 4, "e"}, {0, 15, "c"},
            {1, 24, "g"}, {2, 100, "f"}, {3, 101, "j"}, {4, 456, "h"},
        };
        const std::vector<test_data<int, std::string> > expected_result = {
            {0, 2, "b"}, {1, 3, "d"}, {2, 15, "c"}, {3, 24, "g"}, {4, 101, "j"},
        };

        BSP_tree<int, std::string, std::less<>, 2> tree(path);
        for (const auto &[key, value]: std::vector<std::pair<int, std::string> >{
                 {1, "a"}, {2, "b"}, {15, "c"}, {3, "d"}, {4, "e"},
                 {100, "f"}, {24, "g"}, {456, "h"}, {101, "j"}
             }) {
            tree.emplace(key, value);
        }

        EXPECT_TRUE(infix_const_iterator_test(tree, expected_before_erase));

        tree.erase(1);
        tree.erase(100);
        tree.erase(456);
        tree.erase(4);

        EXPECT_TRUE(infix_const_iterator_test(tree, expected_result));
    }

    std::filesystem::remove(path);
}

TEST(BTreePositiveTests, AtReturnsRequestedValues) {
    const auto path = make_tree_path();

    {
        BSP_tree<int, std::string, std::less<> > tree(path);
        for (const auto &[key, value]: std::vector<std::pair<int, std::string> >{
                 {1, "a"}, {2, "b"}, {15, "c"}, {3, "d"}, {4, "e"}, {100, " "},
                 {24, "g"}, {-456, "h"}, {101, "j"}, {-45, "k"}, {-193, "l"},
                 {534, "m"}, {1000, "y"}
             }) {
            tree.emplace(key, value);
        }

        const std::vector<std::string> expected_result = {"g", "d", "e", " ", "l", "a", "b", "y"};
        const std::vector actual_result = {
            tree.at(24), tree.at(3), tree.at(4), tree.at(100),
            tree.at(-193), tree.at(1), tree.at(2), tree.at(1000)
        };

        EXPECT_TRUE(compare_obtain_results(expected_result, actual_result));
    }

    std::filesystem::remove(path);
}

TEST(BTreePositiveTests, AtWorksWithNegativeAndPositiveKeys) {
    const auto path = make_tree_path();

    {
        BSP_tree<int, std::string, std::less<>, 4> tree(path);
        for (const auto &[key, value]: std::vector<std::pair<int, std::string> >{
                 {1, "a"}, {2, "b"}, {15, "c"}, {3, "d"}, {4, "e"}, {100, " "},
                 {24, "g"}, {-456, "h"}, {101, "j"}, {-45, "k"}, {-193, "l"},
                 {534, "m"}, {1000, "y"}
             }) {
            tree.emplace(key, value);
        }

        const std::vector<std::string> expected_result = {"y", "l", "a", "g", "k", "b", "c", "h"};
        const std::vector actual_result = {
            tree.at(1000), tree.at(-193), tree.at(1), tree.at(24),
            tree.at(-45), tree.at(2), tree.at(15), tree.at(-456)
        };

        EXPECT_TRUE(compare_obtain_results(expected_result, actual_result));
    }

    std::filesystem::remove(path);
}

TEST(BTreePositiveTests, LowerUpperBoundRange) {
    const auto path = make_tree_path();

    {
        const std::vector<BSP_tree<int, std::string>::value_type> expected_result = {
            {4, "e"}, {15, "c"}, {24, "g"}, {45, "k"}, {100, "f"}, {101, "j"},
        };

        BSP_tree<int, std::string, std::less<>, 2> tree(path);
        for (const auto &[key, value]: std::vector<std::pair<int, std::string> >{
                 {1, "a"}, {2, "b"}, {15, "c"}, {3, "d"}, {4, "e"}, {100, "f"},
                 {24, "g"}, {456, "h"}, {101, "j"}, {45, "k"}, {193, "l"}
             }) {
            tree.emplace(key, value);
        }

        const auto b = tree.lower_bound(4);
        const auto e = tree.upper_bound(110);
        const std::vector<decltype(tree)::value_type> actual_result(b, e);

        EXPECT_TRUE((compare_results<int, std::string>(expected_result, actual_result)));
    }

    std::filesystem::remove(path);
}

TEST(BTreeNegativeTests, EraseMissingKeyReturnsEnd) {
    const auto path = make_tree_path();

    {
        BSP_tree<int, std::string, std::less<>, 3> tree(path);
        tree.emplace(1, "a");
        tree.emplace(2, "b");
        tree.emplace(15, "c");
        tree.emplace(3, "d");
        tree.emplace(4, "e");

        EXPECT_EQ(tree.erase(45), tree.end());
    }

    std::filesystem::remove(path);
}

TEST(BTreeNegativeTests, EraseMissingKeyFromMiddleTreeReturnsEnd) {
    const auto path = make_tree_path();

    {
        BSP_tree<int, std::string, std::less<>, 4> tree(path);
        for (const auto &[key, value]: std::vector<std::pair<int, std::string> >{
                 {1, "a"}, {2, "b"}, {15, "c"}, {3, "d"}, {4, "e"}, {100, " "},
                 {24, "g"}, {-456, "h"}, {101, "j"}, {-45, "k"}, {-193, "l"},
                 {534, "m"}, {1000, "y"}
             }) {
            tree.emplace(key, value);
        }

        EXPECT_EQ(tree.erase(1001), tree.end());
    }

    std::filesystem::remove(path);
}

TEST(BStarPlusTreeDiskTests, ReopensRootTermData) {
    const auto path = make_tree_path();

    {
        BSP_tree<int, std::size_t, std::less<>, 2> tree(path);

        EXPECT_TRUE(tree.empty());
        EXPECT_EQ(tree.size(), 0);

        auto [first_it, first_inserted] = tree.emplace(10, 42);
        EXPECT_TRUE(first_inserted);
        EXPECT_EQ(first_it->first, 10);
        EXPECT_EQ(first_it->second, 42);

        auto [second_it, second_inserted] = tree.emplace(5, 11);
        auto [duplicate_it, duplicate_inserted] = tree.emplace(10, 100);

        EXPECT_TRUE(second_inserted);
        EXPECT_FALSE(duplicate_inserted);
        EXPECT_EQ(second_it->first, 5);
        EXPECT_EQ(duplicate_it->second, 42);

        EXPECT_FALSE(tree.empty());
        EXPECT_EQ(tree.size(), 2);
        EXPECT_TRUE(tree.contains(10));
        EXPECT_TRUE(tree.contains(5));
        EXPECT_FALSE(tree.contains(100));
        EXPECT_EQ(tree.at(10), 42);
        EXPECT_EQ(tree.at(5), 11);
    }

    {
        BSP_tree<int, std::size_t, std::less<>, 2> tree(path);

        EXPECT_FALSE(tree.empty());
        EXPECT_EQ(tree.size(), 2);
        EXPECT_TRUE(tree.contains(10));
        EXPECT_TRUE(tree.contains(5));
        EXPECT_FALSE(tree.contains(100));
        EXPECT_EQ(tree.at(10), 42);
        EXPECT_EQ(tree.at(5), 11);
        EXPECT_THROW((void) tree.at(100), std::out_of_range);
    }

    std::filesystem::remove(path);
}

TEST(BStarPlusTreeDiskTests, ReopensSplitRootTermData) {
    const auto path = make_tree_path();

    {
        BSP_tree<int, std::size_t, std::less<>, 2> tree(path);

        for (int key = 20; key >= 0; --key) {
            auto [it, inserted] = tree.emplace(key, static_cast<std::size_t>(key * 10));
            EXPECT_TRUE(inserted);
            EXPECT_EQ(it->first, key);
            EXPECT_EQ(it->second, static_cast<std::size_t>(key * 10));
        }

        EXPECT_EQ(tree.size(), 21);
        for (int key = 0; key <= 20; ++key) {
            EXPECT_TRUE(tree.contains(key));
            EXPECT_EQ(tree.at(key), static_cast<std::size_t>(key * 10));
        }
    }

    {
        BSP_tree<int, std::size_t, std::less<>, 2> tree(path);

        EXPECT_EQ(tree.size(), 21);
        std::vector<int> actual_keys;
        for (const auto &[key, value]: tree) {
            actual_keys.push_back(key);
            EXPECT_EQ(value, static_cast<std::size_t>(key * 10));
        }

        ASSERT_EQ(actual_keys.size(), 21);
        for (int key = 0; key <= 20; ++key) {
            EXPECT_EQ(actual_keys[static_cast<std::size_t>(key)], key);
            EXPECT_TRUE(tree.contains(key));
            EXPECT_EQ(tree.at(key), static_cast<std::size_t>(key * 10));
        }
    }

    std::filesystem::remove(path);
}

TEST(BStarPlusTreeDiskTests, ReopensSplitRootMiddleData) {
    const auto path = make_tree_path();
    constexpr int keys_count = 40;

    {
        BSP_tree<int, std::size_t, std::less<>, 2> tree(path);

        for (int key = 0; key < keys_count; ++key) {
            auto [it, inserted] = tree.emplace(key, static_cast<std::size_t>(key + 1000));
            EXPECT_TRUE(inserted);
            EXPECT_EQ(it->first, key);
            EXPECT_EQ(it->second, static_cast<std::size_t>(key + 1000));
        }

        EXPECT_EQ(tree.size(), keys_count);
    }

    {
        BSP_tree<int, std::size_t, std::less<>, 2> tree(path);

        EXPECT_EQ(tree.size(), keys_count);
        std::vector<int> actual_keys;
        for (const auto &[key, value]: tree) {
            actual_keys.push_back(key);
            EXPECT_EQ(value, static_cast<std::size_t>(key + 1000));
        }

        ASSERT_EQ(actual_keys.size(), keys_count);
        for (int key = 0; key < keys_count; ++key) {
            EXPECT_EQ(actual_keys[static_cast<std::size_t>(key)], key);
            EXPECT_TRUE(tree.contains(key));
            EXPECT_EQ(tree.at(key), static_cast<std::size_t>(key + 1000));
        }
    }

    std::filesystem::remove(path);
}

TEST(BStarPlusTreeDiskTests, ReopensSplitNonRootMiddleData) {
    const auto path = make_tree_path();
    constexpr int keys_count = 120;

    {
        BSP_tree<int, std::size_t, std::less<>, 2> tree(path);

        for (int key = keys_count - 1; key >= 0; --key) {
            auto [it, inserted] = tree.emplace(key, static_cast<std::size_t>(key + 2000));
            EXPECT_TRUE(inserted);
            EXPECT_EQ(it->first, key);
            EXPECT_EQ(it->second, static_cast<std::size_t>(key + 2000));
        }

        EXPECT_EQ(tree.size(), keys_count);
    }

    {
        BSP_tree<int, std::size_t, std::less<>, 2> tree(path);

        EXPECT_EQ(tree.size(), keys_count);
        std::vector<int> actual_keys;
        for (const auto &[key, value]: tree) {
            actual_keys.push_back(key);
            EXPECT_EQ(value, static_cast<std::size_t>(key + 2000));
        }

        ASSERT_EQ(actual_keys.size(), keys_count);
        for (int key = 0; key < keys_count; ++key) {
            EXPECT_EQ(actual_keys[static_cast<std::size_t>(key)], key);
            EXPECT_TRUE(tree.contains(key));
            EXPECT_EQ(tree.at(key), static_cast<std::size_t>(key + 2000));
        }
    }

    std::filesystem::remove(path);
}

TEST(BStarPlusTreeDiskTests, ReopensAfterRootTermErase) {
    const auto path = make_tree_path();

    {
        BSP_tree<int, std::size_t, std::less<>, 2> tree(path);

        for (int key = 0; key < 6; ++key) {
            tree.emplace(key, static_cast<std::size_t>(key + 3000));
        }

        auto next = tree.erase(2);
        EXPECT_NE(next, tree.end());
        EXPECT_EQ(next->first, 3);
        EXPECT_FALSE(tree.contains(2));
        EXPECT_EQ(tree.size(), 5);
    }

    {
        BSP_tree<int, std::size_t, std::less<>, 2> tree(path);

        EXPECT_EQ(tree.size(), 5);
        EXPECT_FALSE(tree.contains(2));
        for (const int key: {0, 1, 3, 4, 5}) {
            EXPECT_TRUE(tree.contains(key));
            EXPECT_EQ(tree.at(key), static_cast<std::size_t>(key + 3000));
        }
    }

    std::filesystem::remove(path);
}

TEST(BStarPlusTreeDiskTests, ReopensAfterMiddleTreeErase) {
    const auto path = make_tree_path();
    constexpr int keys_count = 80;

    {
        BSP_tree<int, std::size_t, std::less<>, 2> tree(path);

        for (int key = 0; key < keys_count; ++key) {
            tree.emplace(key, static_cast<std::size_t>(key + 4000));
        }

        for (const int key: {7, 8, 16, 17, 33, 34, 51, 52, 70}) {
            auto next = tree.erase(key);
            EXPECT_NE(next, tree.end());
            EXPECT_FALSE(tree.contains(key));
        }
        EXPECT_EQ(tree.size(), keys_count - 9);
    }

    {
        BSP_tree<int, std::size_t, std::less<>, 2> tree(path);

        EXPECT_EQ(tree.size(), keys_count - 9);
        std::vector<int> actual_keys;
        for (const auto &[key, value]: tree) {
            actual_keys.push_back(key);
            EXPECT_EQ(value, static_cast<std::size_t>(key + 4000));
            EXPECT_TRUE(tree.contains(key));
            EXPECT_EQ(tree.at(key), static_cast<std::size_t>(key + 4000));
        }

        for (const int key: {7, 8, 16, 17, 33, 34, 51, 52, 70}) {
            EXPECT_FALSE(tree.contains(key));
        }
        ASSERT_EQ(actual_keys.size(), keys_count - 9);
        for (std::size_t i = 1; i < actual_keys.size(); ++i) {
            EXPECT_LT(actual_keys[i - 1], actual_keys[i]);
        }
    }

    std::filesystem::remove(path);
}

TEST(BStarPlusTreeDiskTests, ReopensAfterTermBorrowOnErase) {
    const auto path = make_tree_path();
    constexpr int keys_count = 21;

    {
        BSP_tree<int, std::size_t, std::less<>, 2> tree(path);

        for (int key = 0; key < keys_count; ++key) {
            tree.emplace(key, static_cast<std::size_t>(key + 5000));
        }

        tree.erase(0);
        tree.erase(1);
        EXPECT_EQ(tree.size(), keys_count - 2);
    }

    {
        const BSP_tree<int, std::size_t, std::less<>, 2> tree(path);

        EXPECT_EQ(tree.size(), keys_count - 2);
        EXPECT_FALSE(tree.contains(0));
        EXPECT_FALSE(tree.contains(1));
        for (int key = 2; key < keys_count; ++key) {
            EXPECT_TRUE(tree.contains(key));
            EXPECT_EQ(tree.at(key), static_cast<std::size_t>(key + 5000));
        }
    }

    std::filesystem::remove(path);
}

TEST(BStarPlusTreeDiskTests, ReopensAfterTermMergeOnErase) {
    const auto path = make_tree_path();
    constexpr int keys_count = 21;

    {
        BSP_tree<int, std::size_t, std::less<>, 2> tree(path);

        for (int key = 0; key < keys_count; ++key) {
            tree.emplace(key, static_cast<std::size_t>(key + 6000));
        }

        tree.erase(0);
        tree.erase(1);
        tree.erase(2);
        EXPECT_EQ(tree.size(), keys_count - 3);
    }

    {
        const BSP_tree<int, std::size_t, std::less<>, 2> tree(path);

        EXPECT_EQ(tree.size(), keys_count - 3);
        std::vector<int> actual_keys;
        for (const auto &[key, value]: tree) {
            actual_keys.push_back(key);
            EXPECT_EQ(value, static_cast<std::size_t>(key + 6000));
        }

        ASSERT_EQ(actual_keys.size(), keys_count - 3);
        for (int key = 3; key < keys_count; ++key) {
            EXPECT_EQ(actual_keys[static_cast<std::size_t>(key - 3)], key);
            EXPECT_TRUE(tree.contains(key));
            EXPECT_EQ(tree.at(key), static_cast<std::size_t>(key + 6000));
        }
    }

    std::filesystem::remove(path);
}

TEST(BStarPlusTreeDiskTests, ReopensAfterMiddleRebalanceOnErase) {
    const auto path = make_tree_path();
    constexpr int keys_count = 160;
    constexpr int erased_count = 120;

    {
        BSP_tree<int, std::size_t, std::less<>, 2> tree(path);

        for (int key = 0; key < keys_count; ++key) {
            tree.emplace(key, static_cast<std::size_t>(key + 7000));
        }

        for (int key = 0; key < erased_count; ++key) {
            tree.erase(key);
            EXPECT_FALSE(tree.contains(key));
        }
        EXPECT_EQ(tree.size(), keys_count - erased_count);
    }

    {
        BSP_tree<int, std::size_t, std::less<>, 2> tree(path);

        EXPECT_EQ(tree.size(), keys_count - erased_count);
        std::vector<int> actual_keys;
        for (const auto &[key, value]: tree) {
            actual_keys.push_back(key);
            EXPECT_EQ(value, static_cast<std::size_t>(key + 7000));
        }

        ASSERT_EQ(actual_keys.size(), keys_count - erased_count);
        for (int key = 0; key < erased_count; ++key) {
            EXPECT_FALSE(tree.contains(key));
        }
        for (int key = erased_count; key < keys_count; ++key) {
            EXPECT_EQ(actual_keys[static_cast<std::size_t>(key - erased_count)], key);
            EXPECT_TRUE(tree.contains(key));
            EXPECT_EQ(tree.at(key), static_cast<std::size_t>(key + 7000));
        }
    }

    std::filesystem::remove(path);
}

TEST(BStarPlusTreeDiskTests, ReusesFreedPagesAfterTermMerge) {
    const auto path = make_tree_path();

    {
        BSP_tree<int, std::size_t, std::less<>, 2> tree(path);

        for (int key = 0; key < 21; ++key) {
            tree.emplace(key, static_cast<std::size_t>(key + 8000));
        }
        const auto size_after_split = std::filesystem::file_size(path);

        tree.erase(0);
        tree.erase(1);
        tree.erase(2);
        const auto size_after_merge = std::filesystem::file_size(path);
        EXPECT_EQ(size_after_merge, size_after_split);

        for (int key = 21; key < 25; ++key) {
            tree.emplace(key, static_cast<std::size_t>(key + 8000));
        }
        EXPECT_EQ(std::filesystem::file_size(path), size_after_merge);
    }

    {
        const BSP_tree<int, std::size_t, std::less<>, 2> tree(path);

        for (int key = 0; key < 3; ++key) {
            EXPECT_FALSE(tree.contains(key));
        }
        for (int key = 3; key < 25; ++key) {
            EXPECT_TRUE(tree.contains(key));
            EXPECT_EQ(tree.at(key), static_cast<std::size_t>(key + 8000));
        }
    }

    std::filesystem::remove(path);
}

TEST(BStarPlusTreeDiskTests, ReopensAfterInsertOrAssign) {
    const auto path = make_tree_path();

    {
        BSP_tree<int, std::size_t, std::less<>, 2> tree(path);

        for (int key = 0; key < 30; ++key) {
            tree.emplace(key, static_cast<std::size_t>(key + 9000));
        }

        auto assigned = tree.insert_or_assign({12, 12000});
        EXPECT_EQ(assigned->first, 12);
        EXPECT_EQ(assigned->second, 12000);

        auto emplaced_assigned = tree.emplace_or_assign(24, 24000);
        EXPECT_EQ(emplaced_assigned->first, 24);
        EXPECT_EQ(emplaced_assigned->second, 24000);

        auto inserted = tree.insert_or_assign({100, 100000});
        EXPECT_EQ(inserted->first, 100);
        EXPECT_EQ(inserted->second, 100000);
        EXPECT_EQ(tree.size(), 31);
    }

    {
        BSP_tree<int, std::size_t, std::less<>, 2> tree(path);

        EXPECT_EQ(tree.size(), 31);
        EXPECT_EQ(tree.at(12), 12000);
        EXPECT_EQ(tree.at(24), 24000);
        EXPECT_EQ(tree.at(100), 100000);
        for (const int key: {0, 11, 13, 23, 25, 29}) {
            EXPECT_EQ(tree.at(key), static_cast<std::size_t>(key + 9000));
        }
    }

    std::filesystem::remove(path);
}

TEST(BStarPlusTreeDiskTests, IteratorsExposeReadOnlyDiskCopies) {
    using Tree = BSP_tree<int, std::size_t, std::less<>, 2>;
    static_assert(!std::is_assignable_v<decltype((std::declval<Tree &>().begin()->second)), std::size_t>);

    const auto path = make_tree_path();

    {
        Tree tree(path);
        tree.emplace(1, 10);
        const auto it = tree.begin();
        EXPECT_EQ(it->first, 1);
        EXPECT_EQ(it->second, 10);
    }

    std::filesystem::remove(path);
}

TEST(BStarPlusTreeDiskTests, StoresVariableSizePayloadAndRejectsSinglePageOverflow) {
    const auto path = make_tree_path();
    const std::string variable_size(200, 'x');

    {
        const std::string too_large(5000, 'y');
        BSP_tree<int, std::string, std::less<>, 2> tree(path);
        tree.emplace(1, "small");

        EXPECT_NO_THROW(tree.emplace(2, variable_size));
        EXPECT_NO_THROW(tree.insert_or_assign({1, variable_size}));
        EXPECT_THROW(tree.emplace(3, too_large), std::runtime_error);

        EXPECT_EQ(tree.size(), 2);
        EXPECT_TRUE(tree.contains(1));
        EXPECT_TRUE(tree.contains(2));
        EXPECT_FALSE(tree.contains(3));
        EXPECT_EQ(tree.at(1), variable_size);
        EXPECT_EQ(tree.at(2), variable_size);
    }

    {
        BSP_tree<int, std::string, std::less<>, 2> reopened(path);
        EXPECT_EQ(reopened.size(), 2);
        EXPECT_TRUE(reopened.contains(1));
        EXPECT_TRUE(reopened.contains(2));
        EXPECT_FALSE(reopened.contains(3));
        EXPECT_EQ(reopened.at(1), variable_size);
        EXPECT_EQ(reopened.at(2), variable_size);
    }

    std::filesystem::remove(path);
}

TEST(BStarPlusTreeDiskTests, ClearPersistsEmptyTree) {
    const auto path = make_tree_path();

    {
        BSP_tree<int, std::size_t, std::less<>, 2> tree(path);
        for (int key = 0; key < 40; ++key) {
            tree.emplace(key, static_cast<std::size_t>(key + 10000));
        }

        tree.clear();
        EXPECT_TRUE(tree.empty());
        EXPECT_EQ(tree.size(), 0);
        EXPECT_FALSE(tree.contains(10));

        tree.emplace(100, 10100);
        EXPECT_EQ(tree.size(), 1);
        EXPECT_EQ(tree.at(100), 10100);
    }

    {
        const BSP_tree<int, std::size_t, std::less<>, 2> tree(path);

        EXPECT_EQ(tree.size(), 1);
        EXPECT_FALSE(tree.contains(10));
        EXPECT_TRUE(tree.contains(100));
        EXPECT_EQ(tree.at(100), 10100);
    }

    std::filesystem::remove(path);
}

TEST(BStarPlusTreeDiskTests, MoveConstructedTreeKeepsDiskStorage) {
    const auto path = make_tree_path();

    {
        BSP_tree<int, std::size_t, std::less<>, 2> original(path);
        for (int key = 0; key < 20; ++key) {
            original.emplace(key, static_cast<std::size_t>(key + 11000));
        }

        BSP_tree moved(std::move(original));
        moved.insert_or_assign({5, 50000});
        moved.emplace(30, 30000);
        EXPECT_EQ(moved.at(5), 50000);
        EXPECT_EQ(moved.at(30), 30000);
    }

    {
        const BSP_tree<int, std::size_t, std::less<>, 2> reopened(path);
        EXPECT_EQ(reopened.at(5), 50000);
        EXPECT_EQ(reopened.at(30), 30000);
        EXPECT_EQ(reopened.at(19), 11019);
    }

    std::filesystem::remove(path);
}

TEST(BStarPlusTreeDiskTests, MoveAssignedTreeKeepsDiskStorage) {
    const auto source_path = make_tree_path();
    const auto target_path = make_tree_path();

    {
        BSP_tree<int, std::size_t, std::less<>, 2> source(source_path);
        for (int key = 0; key < 20; ++key) {
            source.emplace(key, static_cast<std::size_t>(key + 12000));
        }

        BSP_tree<int, std::size_t, std::less<>, 2> target(target_path);
        target.emplace(100, 100);
        target = std::move(source);
        target.insert_or_assign({7, 70000});
        target.emplace(40, 40000);
        EXPECT_EQ(target.at(7), 70000);
        EXPECT_EQ(target.at(40), 40000);
    }

    {
        const BSP_tree<int, std::size_t, std::less<>, 2> reopened(source_path);
        EXPECT_EQ(reopened.at(7), 70000);
        EXPECT_EQ(reopened.at(40), 40000);
        EXPECT_EQ(reopened.at(19), 12019);
    }

    {
        const BSP_tree<int, std::size_t, std::less<>, 2> old_target(target_path);
        EXPECT_TRUE(old_target.contains(100));
        EXPECT_FALSE(old_target.contains(7));
    }

    std::filesystem::remove(source_path);
    std::filesystem::remove(target_path);
}

TEST(BStarPlusTreeDiskTests, CopiesDiskBackedTreeToExplicitPath) {
    const auto source_path = make_tree_path();
    const auto target_path = make_tree_path();

    {
        BSP_tree<int, std::size_t, std::less<>, 2> source(source_path);
        for (int key = 0; key < 60; ++key) {
            source.emplace(key, static_cast<std::size_t>(key + 13000));
        }

        BSP_tree<int, std::size_t, std::less<>, 2> target(target_path);
        target.emplace(1000, 1000);

        BSP_tree copied(target_path, source);

        EXPECT_EQ(copied.size(), source.size());
        EXPECT_FALSE(copied.contains(1000));
        for (int key = 0; key < 60; ++key) {
            EXPECT_EQ(copied.at(key), static_cast<std::size_t>(key + 13000));
        }

        copied.insert_or_assign({7, 700000});
        EXPECT_EQ(source.at(7), 13007);
        EXPECT_EQ(copied.at(7), 700000);
    }

    {
        BSP_tree<int, std::size_t, std::less<>, 2> reopened_source(source_path);
        BSP_tree<int, std::size_t, std::less<>, 2> reopened_target(target_path);

        EXPECT_EQ(reopened_source.at(7), 13007);
        EXPECT_EQ(reopened_target.at(7), 700000);
        EXPECT_EQ(reopened_target.size(), 60);
    }

    std::filesystem::remove(source_path);
    std::filesystem::remove(target_path);
}

TEST(BStarPlusTreeDiskTests, CopyingDiskBackedTreeToSamePathThrows) {
    const auto path = make_tree_path();

    {
        BSP_tree<int, std::size_t, std::less<>, 2> source(path);
        source.emplace(1, 10);

        EXPECT_THROW((BSP_tree(path, source)), std::invalid_argument);
        EXPECT_EQ(source.at(1), 10);
    }

    std::filesystem::remove(path);
}

TEST(BStarPlusTreeDiskTests, ThrowsOnCorruptedRootPageType) {
    const auto path = make_tree_path();

    {
        BSP_tree<int, std::size_t, std::less<>, 2> tree(path);
        tree.emplace(1, 10);
    }

    {
        storage::Pager pager(path);
        storage::page_buffer page{};
        pager.read_page(storage::first_data_page_id, page);
        page[0] = std::byte{2};
        pager.write_page(storage::first_data_page_id, page);
    }

    EXPECT_THROW((BSP_tree<int, std::size_t, std::less<>, 2>(path)), std::runtime_error);

    std::filesystem::remove(path);
}

TEST(BStarPlusTreeDiskTests, RandomizedAgainstStdMapWithReopenAndValidate) {
    using Tree = BSP_tree<int, std::string, std::less<>, 2>;

    const auto path = make_tree_path();
    std::map<int, std::string, std::less<> > expected;
    // NOLINTNEXTLINE(cert-msc32-c, cert-msc51-cpp)
    std::mt19937 rng(0xB57A2025);
    std::uniform_int_distribution key_dist(-250, 250);
    std::uniform_int_distribution op_dist(0, 7);

    const auto value_for = [](const int step, const int key) {
        return "value_" + std::to_string(step) + "_" + std::to_string(key);
    };

    for (int batch = 0; batch < 25; ++batch) {
        Tree tree(path);
        expect_tree_equals(tree, expected);

        for (int step = 0; step < 120; ++step) {
            const int absolute_step = batch * 120 + step;
            const int key = key_dist(rng);
            const std::string value = value_for(absolute_step, key);

            switch (op_dist(rng)) {
                case 0: {
                    const auto [it, inserted] = tree.emplace(key, value);
                    const auto [expected_it, expected_inserted] = expected.emplace(key, value);
                    EXPECT_EQ(inserted, expected_inserted);
                    EXPECT_EQ(it->first, expected_it->first);
                    EXPECT_EQ(it->second, expected_it->second);
                    break;
                }
                case 1: {
                    const auto it = tree.insert_or_assign({key, value});
                    expected[key] = value;
                    EXPECT_EQ(it->first, key);
                    EXPECT_EQ(it->second, value);
                    break;
                }
                case 2: {
                    tree.insert_or_assign({key, value});
                    expected[key] = value;
                    EXPECT_EQ(tree.at(key), value);
                    break;
                }
                case 3: {
                    const bool had_key = expected.contains(key);
                    const auto next = tree.erase(key);
                    const auto expected_next = expected.upper_bound(key);
                    expected.erase(key);

                    if (!had_key || expected_next == expected.end()) {
                        EXPECT_EQ(next, tree.end());
                    } else {
                        ASSERT_NE(next, tree.end());
                        EXPECT_EQ(next->first, expected_next->first);
                        EXPECT_EQ(next->second, expected_next->second);
                    }
                    break;
                }
                case 4: {
                    EXPECT_EQ(tree.contains(key), expected.contains(key));
                    if (const auto it = expected.find(key); it == expected.end()) {
                        EXPECT_THROW((void) tree.at(key), std::out_of_range);
                    } else {
                        EXPECT_EQ(tree.at(key), it->second);
                    }
                    break;
                }
                case 5: {
                    const auto tree_it = tree.lower_bound(key);
                    if (const auto map_it = expected.lower_bound(key); map_it == expected.end()) {
                        EXPECT_EQ(tree_it, tree.end());
                    } else {
                        ASSERT_NE(tree_it, tree.end());
                        EXPECT_EQ(tree_it->first, map_it->first);
                        EXPECT_EQ(tree_it->second, map_it->second);
                    }
                    break;
                }
                case 6: {
                    const auto tree_it = tree.upper_bound(key);
                    if (const auto map_it = expected.upper_bound(key); map_it == expected.end()) {
                        EXPECT_EQ(tree_it, tree.end());
                    } else {
                        ASSERT_NE(tree_it, tree.end());
                        EXPECT_EQ(tree_it->first, map_it->first);
                        EXPECT_EQ(tree_it->second, map_it->second);
                    }
                    break;
                }
                default: {
                    const auto [inserted_fst, inserted_snd] = tree.insert({key, value});
                    const auto [expected_fst, expected_snd] = expected.insert({key, value});
                    EXPECT_EQ(inserted_snd, expected_snd);
                    EXPECT_EQ(inserted_fst->first, expected_fst->first);
                    EXPECT_EQ(inserted_fst->second, expected_fst->second);
                    break;
                }
            }

            if (absolute_step % 25 == 0) {
                expect_tree_equals(tree, expected);
            }
        }

        expect_tree_equals(tree, expected);
    }

    {
        const Tree reopened(path);
        expect_tree_equals(reopened, expected);
    }

    std::filesystem::remove(path);
}
