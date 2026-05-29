#pragma once

#include "page.hpp"
#include "serializer.hpp"

#include <span>
#include <vector>

namespace storage {
    class PageWriter {
    public:
        explicit PageWriter(page_buffer &page, std::size_t offset = 0) noexcept;

        template<typename T>
        void write_trivial(const T &value);

        template<typename... Ts>
        void write_trivial_many(const Ts &...values);

        template<typename T>
        void write_serialized(const T &value, std::size_t slot_size);

        void write_bytes(std::span<const std::byte> bytes);

        void write_sized_bytes(std::span<const std::byte> bytes);

        [[nodiscard]] std::size_t offset() const noexcept;

        void seek(std::size_t offset);

    private:
        page_buffer &page_;
        std::size_t offset_;

        void check_available(std::size_t bytes) const;
    };

    class PageReader {
    public:
        explicit PageReader(const page_buffer &page, std::size_t offset = 0) noexcept;

        template<typename T>
        [[nodiscard]] T read_trivial();

        template<typename... Ts>
        void read_trivial_into(Ts &...values);

        template<typename T>
        [[nodiscard]] T read_serialized(std::size_t slot_size);

        void read_bytes(std::span<std::byte> out);

        [[nodiscard]] std::vector<std::byte> read_sized_bytes();

        [[nodiscard]] std::size_t offset() const noexcept;

        void seek(std::size_t offset);

    private:
        const page_buffer &page_;
        std::size_t offset_;

        void ensure_available(std::size_t bytes) const;
    };

    template<typename T>
    void PageWriter::write_trivial(const T &value) {
        write_serialized(value, sizeof(T));
    }

    template<typename... Ts>
    void PageWriter::write_trivial_many(const Ts &...values) {
        (write_trivial(values), ...);
    }

    template<typename T>
    void PageWriter::write_serialized(const T &value, const std::size_t slot_size) {
        check_available(slot_size);
        Serializer<T>::write(std::span<std::byte>(page_).subspan(offset_, slot_size), value);
        offset_ += slot_size;
    }

    template<typename T>
    T PageReader::read_trivial() {
        return read_serialized<T>(sizeof(T));
    }

    template<typename... Ts>
    void PageReader::read_trivial_into(Ts &...values) {
        ((values = read_trivial<Ts>()), ...);
    }

    template<typename T>
    T PageReader::read_serialized(const std::size_t slot_size) {
        ensure_available(slot_size);
        auto value = Serializer<T>::read(std::span<const std::byte>(page_).subspan(offset_, slot_size));
        offset_ += slot_size;
        return value;
    }
} // namespace storage
