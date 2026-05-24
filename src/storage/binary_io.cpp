#include "storage/binary_io.hpp"

#include "not_implemented.h"

namespace storage {

void write_value(const std::ostream& /*out*/, const Value& /*value*/) {
    throw not_implemented("storage::write_value(std::ostream&, const Value&)", "binary value serialization skeleton");
}

Value read_value(const std::istream& /*in*/) {
    throw not_implemented("storage::read_value(std::istream&)", "binary value deserialization skeleton");
}

void write_row(const std::ostream& /*out*/, const Row& /*row*/) {
    throw not_implemented("storage::write_row(std::ostream&, const Row&)", "binary row serialization skeleton");
}

Row read_row(const std::istream& /*in*/, const std::size_t /*column_count*/) {
    throw not_implemented("storage::read_row(std::istream&, std::size_t)", "binary row deserialization skeleton");
}

} // namespace storage
