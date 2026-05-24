#pragma once

#include "not_implemented.h"

#include <cstddef>
#include <span>

namespace storage {
    template<typename T>
    struct Serializer {
        static void write(std::span<std::byte> out, const T &value);

        static T read(std::span<const std::byte> in);
    };

    template<typename T>
    void Serializer<T>::write(const std::span<std::byte> /*out*/, const T &/*value*/) {
        throw not_implemented("storage::Serializer<T>::write(std::span<std::byte>, const T&)", "generic serializer skeleton");
    }

    template<typename T>
    T Serializer<T>::read(const std::span<const std::byte> /*in*/) {
        throw not_implemented("storage::Serializer<T>::read(std::span<const std::byte>)", "generic serializer skeleton");
    }
} // namespace storage
