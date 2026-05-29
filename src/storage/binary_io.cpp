#include "storage/binary_io.hpp"

#include <istream>
#include <ostream>
#include <stdexcept>

namespace storage {

namespace {

void read_safe(std::istream& in, char* data, const std::streamsize size) {
    if (!in.read(data, size)) {
        throw std::runtime_error("Corrupted binary data: unexpected end of file");
    }
}

void write_safe(std::ostream& out, const char* data, const std::streamsize size) {
    if (!out.write(data, size)) {
        throw std::runtime_error("Failed to write binary data");
    }
}

} // namespace

void write_value(std::ostream& out, const Value& value) {
    const auto type_idx = static_cast<char>(value.index());
    write_safe(out, &type_idx, sizeof(type_idx));

    if (std::holds_alternative<int>(value)) {
        const auto val = std::get<int>(value);
        write_safe(out, reinterpret_cast<const char*>(&val), sizeof(val));
        return;
    }

    if (std::holds_alternative<InternedString>(value)) {
        const auto& interned = std::get<InternedString>(value);
        const auto& val = global_string_pool().get(interned.id);
        const auto len = val.size();
        write_safe(out, reinterpret_cast<const char*>(&len), sizeof(len));
        write_safe(out, val.data(), static_cast<std::streamsize>(len));
        return;
    }

    if (std::holds_alternative<std::nullptr_t>(value)) {
        return;
    }

    throw std::runtime_error("Unsupported value type");
}

Value read_value(std::istream& in) {
    char type_idx{};
    read_safe(in, &type_idx, sizeof(type_idx));

    if (type_idx == 0) {
        int val{};
        read_safe(in, reinterpret_cast<char*>(&val), sizeof(val));
        return val;
    }

    if (type_idx == 1) {
        std::size_t len{};
        read_safe(in, reinterpret_cast<char*>(&len), sizeof(len));

        std::string val(len, '\0');
        read_safe(in, val.data(), static_cast<std::streamsize>(len));
        return InternedString{global_string_pool().intern(val)};
    }

    if (type_idx == 2) {
        return nullptr;
    }

    throw std::runtime_error("Corrupted binary data: unknown value type");
}

void write_row(std::ostream& out, const Row& row) {
    for (const auto& value : row) {
        write_value(out, value);
    }
}

Row read_row(std::istream& in, const std::size_t column_count) {
    Row row;
    row.reserve(column_count);

    for (std::size_t i = 0; i < column_count; ++i) {
        row.push_back(read_value(in));
    }

    return row;
}

} // namespace storage
