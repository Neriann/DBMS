#pragma once

#include "core/row.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace storage {
    namespace detail {
        inline void write_payload(std::span<std::byte> out, const std::string &payload) {
            if (out.size() < payload.size()) throw std::runtime_error("Serializer buffer is too small");
            std::ranges::fill(out, std::byte{0});
            std::memcpy(out.data(), payload.data(), payload.size());
        }

        inline std::string span_to_string(const std::span<const std::byte> in) {
            return {reinterpret_cast<const char *>(in.data()), in.size()};
        }
    } // namespace detail

    template<typename T>
    struct Serializer {
        static void write(std::span<std::byte> out, const T &value);

        static T read(std::span<const std::byte> in);
    };

    template<typename T>
    void Serializer<T>::write(const std::span<std::byte> out, const T &value) {
        static_assert(std::is_trivially_copyable_v<T>,
                      "storage::Serializer<T> supports only trivially copyable types by default; "
                      "add a specialization for this type");
        if (out.size() < sizeof(T)) throw std::runtime_error("Serializer buffer is too small");
        std::ranges::fill(out, std::byte{0});
        std::memcpy(out.data(), &value, sizeof(T));
    }

    template<typename T>
    T Serializer<T>::read(const std::span<const std::byte> in) {
        static_assert(std::is_trivially_copyable_v<T>,
                      "storage::Serializer<T> supports only trivially copyable types by default; "
                      "add a specialization for this type");
        if (in.size() < sizeof(T)) throw std::runtime_error("Serializer buffer is too small");

        T value{};
        std::memcpy(&value, in.data(), sizeof(T));
        return value;
    }

    template<>
    struct Serializer<std::string> {
        static void write(std::span<std::byte> out, const std::string &value);

        static std::string read(std::span<const std::byte> in);
    };

    template<>
    struct Serializer<Value> {
        static void write(std::span<std::byte> out, const Value &value);

        static Value read(std::span<const std::byte> in);
    };

    template<>
    struct Serializer<Row> {
        static void write(std::span<std::byte> out, const Row &row);

        static Row read(std::span<const std::byte> in);
    };
} // namespace storage
