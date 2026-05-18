#include "core/table.hpp"
#include "core/schema.hpp"
#include "core/index_tree.hpp"
#include <ranges>
#include <stdexcept>

struct Index {
    std::size_t col_index;
    IndexTree<Value, RowID, ValueComparator> tree;

    explicit Index(const std::size_t col_index) : col_index(col_index) {
    }
};

Table::Table(Schema schema) : schema_(std::move(schema)) {
    for (size_t i = 0; i < schema_.size(); ++i) {
        if (schema_[i].is_indexed()) {
            indexes_[schema_[i].name] = std::make_unique<Index>(i);
        }
    }
}

Table::~Table() = default;

bool Table::is_deleted(const RowID id) const {
    if (id >= deleted_.size()) {
        throw std::out_of_range("RowID out of range");
    }
    return deleted_[id];
}

void Table::validate_row(const Row &row) const {
    if (row.size() != schema_.size()) {
        throw std::invalid_argument("Row size does not match schema");
    }

    for (size_t i = 0; i < schema_.size(); ++i) {
        const auto &val = row[i];
        const auto &col = schema_[i];

        if (std::holds_alternative<std::nullptr_t>(val)) {
            if (col.is_not_null() || col.is_indexed()) {
                throw std::invalid_argument("Column '" + col.name + "' cannot be NULL");
            }
        } else if (col.type == ColumnType::INT && !std::holds_alternative<int>(val)) {
            throw std::invalid_argument("Column '" + col.name + "' expects INT");
        } else if (col.type == ColumnType::STRING && !std::holds_alternative<std::string>(val)) {
            throw std::invalid_argument("Column '" + col.name + "' expects STRING");
        }
    }
}

RowID Table::insert(const Row &row) {
    validate_row(row);
    for (auto &[col_name, index]: indexes_) {
        if (index->tree.contains(row[index->col_index])) {
            throw std::invalid_argument("Duplicate value in INDEXED column '" + col_name + "'");
        }
    }

    RowID id = data_.size();
    data_.push_back(row);
    deleted_.push_back(false);
    for (const auto &index: indexes_ | std::views::values) {
        index->tree.insert({row[index->col_index], id});
    }

    return id;
}

void Table::restore_row(Row row, const bool deleted) {
    validate_row(row);

    if (!deleted) {
        for (auto &[col_name, index]: indexes_) {
            if (index->tree.contains(row[index->col_index])) {
                throw std::invalid_argument("Duplicate value in INDEXED column '" + col_name + "'");
            }
        }
    }

    const RowID id = data_.size();
    data_.push_back(std::move(row));
    deleted_.push_back(deleted);

    if (!deleted) {
        for (const auto &index: indexes_ | std::views::values) {
            index->tree.insert({data_[id][index->col_index], id});
        }
    }
}

void Table::update(const RowID id, Row row) {
    if (id >= data_.size() || deleted_[id]) {
        throw std::out_of_range("RowID out of range or deleted");
    }
    validate_row(row);

    for (auto &[col_name, index]: indexes_) {
        if (row[index->col_index] != data_[id][index->col_index] && index->tree.contains(row[index->col_index])) {
            throw std::invalid_argument("Duplicate value in INDEXED column '" + col_name + "'");
        }
    }

    for (const auto &index: indexes_ | std::views::values) {
        index->tree.erase(data_[id][index->col_index]);
        index->tree.insert({row[index->col_index], id});
    }

    data_[id] = std::move(row);
}

void Table::erase(const RowID id) {
    if (id >= data_.size() || deleted_[id]) {
        throw std::out_of_range("RowID out of range or deleted");
    }

    for (const auto &index: indexes_ | std::views::values) {
        index->tree.erase(data_[id][index->col_index]);
    }
    deleted_[id] = true;
}
