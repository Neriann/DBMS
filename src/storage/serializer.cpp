#include "storage/serializer.hpp"

#include "storage/binary_io.hpp"

#include <sstream>

namespace storage {
    void Serializer<std::string>::write(const std::span<std::byte> out, const std::string &value) {
        std::ostringstream stream(std::ios::binary);
        const auto size = value.size();
        stream.write(reinterpret_cast<const char *>(&size), sizeof(size));
        stream.write(value.data(), static_cast<std::streamsize>(value.size()));

        if (!stream) throw std::runtime_error("Failed to serialize std::string");

        detail::write_payload(out, stream.str());
    }

    std::string Serializer<std::string>::read(const std::span<const std::byte> in) {
        std::istringstream stream(detail::span_to_string(in), std::ios::binary);

        std::size_t size{};
        if (!stream.read(reinterpret_cast<char *>(&size), sizeof(size))) {
            throw std::runtime_error("Corrupted string data: unexpected end of buffer");
        }

        std::string value(size, '\0');
        if (!stream.read(value.data(), static_cast<std::streamsize>(size))) {
            throw std::runtime_error("Corrupted string data: unexpected end of buffer");
        }

        return value;
    }

    void Serializer<Value>::write(const std::span<std::byte> out, const Value &value) {
        std::ostringstream stream(std::ios::binary);
        write_value(stream, value);
        detail::write_payload(out, stream.str());
    }

    Value Serializer<Value>::read(const std::span<const std::byte> in) {
        std::istringstream stream(detail::span_to_string(in), std::ios::binary);
        return read_value(stream);
    }

    void Serializer<Row>::write(const std::span<std::byte> out, const Row &row) {
        std::ostringstream stream(std::ios::binary);
        const auto column_count = row.size();
        stream.write(reinterpret_cast<const char *>(&column_count), sizeof(column_count));
        write_row(stream, row);

        if (!stream) throw std::runtime_error("Failed to serialize Row");

        detail::write_payload(out, stream.str());
    }

    Row Serializer<Row>::read(const std::span<const std::byte> in) {
        std::istringstream stream(detail::span_to_string(in), std::ios::binary);

        std::size_t column_count{};
        if (!stream.read(reinterpret_cast<char *>(&column_count), sizeof(column_count))) {
            throw std::runtime_error("Corrupted row data: unexpected end of buffer");
        }

        return read_row(stream, column_count);
    }
} // namespace storage
