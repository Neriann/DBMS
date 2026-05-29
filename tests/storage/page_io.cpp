#include "storage/page_io.hpp"

#include "gtest/gtest.h"

#include <array>
#include <string>

TEST(PageIO, WritesAndReadsTrivialValuesSequentially) {
    storage::page_buffer page{};
    storage::PageWriter writer(page);

    writer.write_trivial<std::uint32_t>(0x12345678u);
    writer.write_trivial<std::uint16_t>(0x90abu);

    EXPECT_EQ(writer.offset(), sizeof(std::uint32_t) + sizeof(std::uint16_t));

    storage::PageReader reader(page);

    EXPECT_EQ(reader.read_trivial<std::uint32_t>(), 0x12345678u);
    EXPECT_EQ(reader.read_trivial<std::uint16_t>(), 0x90abu);
    EXPECT_EQ(reader.offset(), writer.offset());
}

TEST(PageIO, WritesAndReadsSerializedValuesInFixedSlots) {
    storage::page_buffer page{};
    storage::PageWriter writer(page);

    writer.write_serialized<std::string>("hello", 64);
    writer.write_serialized<Value>(Value{42}, 32);
    writer.write_serialized<Row>(Row{1, std::string("abc"), nullptr}, 128);

    storage::PageReader reader(page);

    EXPECT_EQ(reader.read_serialized<std::string>(64), "hello");
    EXPECT_EQ(reader.read_serialized<Value>(32), Value{42});
    EXPECT_EQ(reader.read_serialized<Row>(128), (Row{1, std::string("abc"), nullptr}));
}

TEST(PageIO, SupportsSeekAndRawBytes) {
    storage::page_buffer page{};
    storage::PageWriter writer(page);
    constexpr std::array expected{std::byte{0x01}, std::byte{0x02}, std::byte{0x03}};

    writer.seek(10);
    writer.write_bytes(expected);

    std::array<std::byte, expected.size()> actual{};
    storage::PageReader reader(page);
    reader.seek(10);
    reader.read_bytes(actual);

    EXPECT_EQ(actual, expected);
}

TEST(PageIO, ThrowsWhenCrossingPageBoundary) {
    storage::page_buffer page{};
    storage::PageWriter writer(page, storage::page_size - 1);
    storage::PageReader reader(page, storage::page_size - 1);

    EXPECT_THROW(writer.write_trivial<std::uint16_t>(1), std::out_of_range);
    EXPECT_THROW(static_cast<void>(reader.read_trivial<std::uint16_t>()), std::out_of_range);
}
