#pragma once

#include "core/row.hpp"

#include <iosfwd>

namespace storage {

void write_value(std::ostream& out, const Value& value);
Value read_value(std::istream& in);

void write_row(std::ostream& out, const Row& row);
Row read_row(std::istream& in, std::size_t column_count);

} // namespace storage
