#include "storage/serializer.hpp"

#include "gtest/gtest.h"

#include <array>
#include <cstddef>
#include <string>

TEST(Serializer, RoundTripsTriviallyCopyableValue) {
    std::array<std::byte, sizeof(int)> buffer{};

    storage::Serializer<int>::write(buffer, 42);

    EXPECT_EQ(storage::Serializer<int>::read(buffer), 42);
}

TEST(Serializer, RoundTripsString) {
    std::array<std::byte, 64> buffer{};

    storage::Serializer<std::string>::write(buffer, "hello");

    EXPECT_EQ(storage::Serializer<std::string>::read(buffer), "hello");
}

TEST(Serializer, RoundTripsValue) {
    std::array<std::byte, 64> buffer{};
    const Value expected{std::string("indexed value")};

    storage::Serializer<Value>::write(buffer, expected);

    EXPECT_EQ(storage::Serializer<Value>::read(buffer), expected);
}

TEST(Serializer, RoundTripsRow) {
    std::array<std::byte, 128> buffer{};
    const Row expected{1, std::string("abc"), nullptr};

    storage::Serializer<Row>::write(buffer, expected);

    EXPECT_EQ(storage::Serializer<Row>::read(buffer), expected);
}

TEST(Serializer, ThrowsWhenBufferIsTooSmall) {
    std::array<std::byte, 2> buffer{};

    EXPECT_THROW(storage::Serializer<int>::write(buffer, 42), std::runtime_error);
    EXPECT_THROW(storage::Serializer<std::string>::write(buffer, "hello"), std::runtime_error);
}
