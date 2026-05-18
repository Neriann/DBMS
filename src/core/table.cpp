#include "core/table.hpp"
#include "core/schema.hpp"
#include "core/index_tree.hpp"
#include <ranges>
#include <set>
#include <stdexcept>
#include <unordered_set>

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
    return insert_many({row}).front();
}

std::vector<RowID> Table::insert_many(const std::vector<Row> &rows) {
    for (const auto &row: rows) {
        validate_row(row);
    }

    for (auto &[col_name, index]: indexes_) {
        std::set<Value, ValueComparator> batch_values;
        for (const auto &row: rows) {
            if (const auto &value = row[index->col_index];
                !batch_values.insert(value).second || index->tree.contains(value)) {
                throw std::invalid_argument("Duplicate value in INDEXED column '" + col_name + "'");
            }
        }
    }

    const RowID first_id = data_.size();
    std::vector<RowID> ids;
    ids.reserve(rows.size());
    data_.reserve(data_.size() + rows.size());
    deleted_.reserve(deleted_.size() + rows.size());

    for (const auto &row: rows) {
        const RowID id = data_.size();
        ids.push_back(id);
        data_.push_back(row);
        deleted_.push_back(false);
    }

    for (RowID id = first_id; id < data_.size(); ++id) {
        for (const auto &index: indexes_ | std::views::values) {
            index->tree.insert({data_[id][index->col_index], id});
        }
    }

    return ids;
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
    update_many({{id, std::move(row)}});
}

void Table::update_many(std::vector<std::pair<RowID, Row> > updates) {
    std::unordered_set<RowID> updated_ids;
    updated_ids.reserve(updates.size());

    for (const auto &[id, row]: updates) {
        if (id >= data_.size() || deleted_[id]) throw std::out_of_range("RowID out of range or deleted");
        if (!updated_ids.insert(id).second) throw std::invalid_argument("Duplicate RowID in update batch");
        validate_row(row);
    }

    for (auto &[col_name, index]: indexes_) {
        std::set<Value, ValueComparator> final_values;
        for (const auto &row: updates | std::views::values) {
            if (const auto &value = row[index->col_index]; !final_values.insert(value).second) {
                throw std::invalid_argument("Duplicate value in INDEXED column '" + col_name + "'");
            }
        }

        for (RowID id = 0; id < data_.size(); ++id) {
            if (!deleted_[id] && !updated_ids.contains(id) && final_values.contains(data_[id][index->col_index])) {
                throw std::invalid_argument("Duplicate value in INDEXED column '" + col_name + "'");
            }
        }
    }

    for (const auto &id: updates | std::views::keys) {
        for (const auto &index: indexes_ | std::views::values) {
            index->tree.erase(data_[id][index->col_index]);
        }
    }

    for (auto &[id, row]: updates) {
        data_[id] = std::move(row);
        for (const auto &index: indexes_ | std::views::values) {
            index->tree.insert({data_[id][index->col_index], id});
        }
    }
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
