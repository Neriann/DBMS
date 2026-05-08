#include "core/Table.hpp"
#include "core/index_tree.hpp"
#include <stdexcept>
#include "not_implemented.h"

struct Index {
    IndexTree<Value, RowID, ValueComparator> tree;
};

Table::Table(Schema schema) : schema_(std::move(schema)) {
    for (const auto &col: schema_) {
        if (col.is_indexed()) indexes_[col.name] = std::make_unique<Index>();
    }
}

Table::~Table() = default;

void Table::validate_row(const Row &/*row*/) const {
    throw not_implemented("void Table::validate_row(const Row &row) const", "is not implemented");
}

RowID Table::insert(const Row &/*row*/) {
    throw not_implemented("RowID Table::insert(const Row &row)", "is not implemented");
}

void Table::update(const RowID /*id*/, Row /*row*/) {
    throw not_implemented("void Table::update(const RowID id, Row row)", "is not implemented");
}

void Table::erase(const RowID /*id*/) {
    throw not_implemented("void Table::erase(const RowID id)", "is not implemented");
}
