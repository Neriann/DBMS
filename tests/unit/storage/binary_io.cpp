#include "storage/binary_io.hpp"
#include "value_test_utils.hpp"

#include "gtest/gtest.h"

#include <sstream>

TEST(BinaryIO, RoundTripsValues) {
    std::stringstream stream(std::ios::in | std::ios::out | std::ios::binary);

    storage::write_value(stream, 42);
    storage::write_value(stream, interned_value("hello"));
    storage::write_value(stream, nullptr);

    EXPECT_EQ(storage::read_value(stream), Value{42});
    EXPECT_EQ(interned_string(storage::read_value(stream)), "hello");
    EXPECT_EQ(storage::read_value(stream), Value{nullptr});
}

TEST(BinaryIO, RoundTripsRows) {
    std::stringstream stream(std::ios::in | std::ios::out | std::ios::binary);
    const Row expected{1, interned_value("abc"), nullptr};

    storage::write_row(stream, expected);

    EXPECT_EQ(storage::read_row(stream, expected.size()), expected);
}

TEST(BinaryIO, ThrowsOnTruncatedInput) {
    std::stringstream stream(std::ios::in | std::ios::out | std::ios::binary);
    storage::write_value(stream, interned_value("abc"));

    auto bytes = stream.str();
    bytes.pop_back();

    std::stringstream truncated(bytes, std::ios::in | std::ios::out | std::ios::binary);

    EXPECT_THROW(static_cast<void>(storage::read_value(truncated)), std::runtime_error);
}
