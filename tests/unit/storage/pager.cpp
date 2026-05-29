#include "storage/pager.hpp"

#include "gtest/gtest.h"

#include <chrono>
#include <filesystem>

namespace {

std::filesystem::path unique_pager_path() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() / ("dbms_pager_test_" + std::to_string(now) + ".bin");
}

} // namespace

TEST(Pager, AllocatesZeroFilledPages) {
    const auto path = unique_pager_path();
    storage::Pager pager(path);

    const auto first = pager.allocate_page();
    const auto second = pager.allocate_page();

    EXPECT_EQ(first, storage::header_page_id);
    EXPECT_EQ(second, storage::first_data_page_id);
    EXPECT_EQ(std::filesystem::file_size(path), storage::page_size * 2);

    storage::page_buffer page{};
    page.fill(std::byte{0x7f});
    pager.read_page(first, page);

    for (const auto byte : page) {
        EXPECT_EQ(byte, std::byte{0});
    }

    std::filesystem::remove(path);
}

TEST(Pager, WritesAndReadsPageById) {
    const auto path = unique_pager_path();
    storage::Pager pager(path);

    const auto first = pager.allocate_page();
    const auto second = pager.allocate_page();

    storage::page_buffer expected{};
    expected[0] = std::byte{0x12};
    expected[100] = std::byte{0x34};
    expected.back() = std::byte{0x56};

    pager.write_page(second, expected);

    storage::page_buffer actual{};
    pager.read_page(second, actual);

    EXPECT_EQ(actual, expected);

    storage::page_buffer first_page{};
    pager.read_page(first, first_page);
    EXPECT_EQ(first_page, storage::page_buffer{});

    std::filesystem::remove(path);
}

TEST(Pager, ThrowsWhenReadingMissingPage) {
    const auto path = unique_pager_path();
    storage::Pager pager(path);

    storage::page_buffer page{};

    EXPECT_THROW(pager.read_page(storage::header_page_id, page), std::out_of_range);

    std::filesystem::remove(path);
}
