#include "core/table.hpp"
#include "core/schema.hpp"
#include "core/trees/b_star_plus_tree.hpp"
#include <chrono>
#include <filesystem>
#include <ranges>
#include <set>
#include <stdexcept>
#include <unordered_set>
#include <utility>

struct Index {
    std::size_t col_index;
    BSP_tree<Value, RowID, ValueComparator> tree;

    Index(const std::size_t col_index, const std::filesystem::path &path) : col_index(col_index), tree(path) {
    }
};

namespace {
    std::filesystem::path make_runtime_indexes_dir() {
        static std::size_t counter = 0;
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        auto path = std::filesystem::temp_directory_path() / "dbms_bsp_tree_indexes";
        path /= "table_" + std::to_string(stamp) + "_" + std::to_string(++counter);
        std::filesystem::create_directories(path);
        return path;
    }
}

Table::Table(Schema schema) : Table(std::move(schema), make_runtime_indexes_dir()) {
}

Table::Table(Schema schema, std::filesystem::path indexes_dir)
    : schema_(std::move(schema)), indexes_dir_(std::move(indexes_dir)) {
    std::filesystem::create_directories(indexes_dir_);
    for (size_t i = 0; i < schema_.size(); ++i) {
        if (schema_[i].is_indexed()) {
            indexes_[schema_[i].name] = std::make_unique<Index>(i, indexes_dir_ / (schema_[i].name + ".idx"));
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

Table::TimestampMillis Table::current_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
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
        } else if (col.type == ColumnType::STRING && !std::holds_alternative<InternedString>(val)) {
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

    const TimestampMillis timestamp_ms = current_time_ms();
    for (RowID id = first_id; id < data_.size(); ++id) {
        append_version(id, data_[id], false, timestamp_ms);
    }

    return ids;
}

void Table::restore_row(Row row, const bool deleted) {
    if (deleted && row.size() != schema_.size()) {
        throw std::invalid_argument("Row size does not match schema");
    }
    if (!deleted) {
        validate_row(row);
    }

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

    const TimestampMillis timestamp_ms = current_time_ms();
    for (auto &[id, row]: updates) {
        data_[id] = std::move(row);
        for (const auto &index: indexes_ | std::views::values) {
            index->tree.insert({data_[id][index->col_index], id});
        }
        append_version(id, data_[id], false, timestamp_ms);
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
    append_version(id, data_[id], true, current_time_ms());
}

void Table::append_version(const RowID id, const Row &row, const bool deleted, const TimestampMillis timestamp_ms) {
    history_.push_back(RowVersion{id, timestamp_ms, row, deleted});
}

void Table::restore_history(std::vector<RowVersion> history) {
    history_ = std::move(history);
}

void Table::rebuild_indexes() {
    indexes_.clear();
    for (size_t i = 0; i < schema_.size(); ++i) {
        if (schema_[i].is_indexed()) {
            const auto index_path = indexes_dir_ / (schema_[i].name + ".idx");
            std::filesystem::remove(index_path);
            indexes_[schema_[i].name] = std::make_unique<Index>(i, index_path);
        }
    }

    for (RowID id = 0; id < data_.size(); ++id) {
        if (deleted_[id]) {
            continue;
        }
        for (auto &[col_name, index]: indexes_) {
            const auto &value = data_[id][index->col_index];
            if (index->tree.contains(value)) {
                throw std::invalid_argument("Duplicate value in INDEXED column '" + col_name + "'");
            }
            index->tree.insert({value, id});
        }
    }
}

std::size_t Table::revert_to(const TimestampMillis timestamp_ms) {
    struct State {
        Row row;
        bool deleted = true;
        bool exists = false;
    };

    std::vector<State> states(data_.size());
    for (const RowVersion &version: history_) {
        if (version.timestamp_ms > timestamp_ms) {
            continue;
        }
        if (version.row_id >= states.size()) {
            states.resize(version.row_id + 1);
        }
        states[version.row_id] = State{version.row, version.deleted, true};
    }

    std::size_t changed = 0;
    const std::size_t old_size = data_.size();
    const std::vector<Row> old_data = data_;
    const TimestampMillis now_ms = current_time_ms();
    for (RowID id = 0; id < states.size(); ++id) {
        const bool old_exists = id < old_size;
        const bool old_deleted = !old_exists || deleted_[id];
        const Row old_row = old_exists ? data_[id] : Row{};

        const bool new_deleted = !states[id].exists || states[id].deleted;
        const Row &new_row = states[id].row;

        const bool differs = old_deleted != new_deleted
            || (!old_deleted && !new_deleted && old_row != new_row);
        if (differs) {
            ++changed;
        }
    }

    data_.clear();
    deleted_.clear();
    data_.reserve(states.size());
    deleted_.reserve(states.size());
    for (const State &state: states) {
        if (state.exists) {
            validate_row(state.row);
            data_.push_back(state.row);
            deleted_.push_back(state.deleted);
        } else {
            data_.push_back(data_.size() < old_size ? old_data[data_.size()] : Row(schema_.size(), nullptr));
            deleted_.push_back(true);
        }
    }

    rebuild_indexes();

    if (changed > 0) {
        for (RowID id = 0; id < data_.size(); ++id) {
            append_version(id, data_[id], deleted_[id], now_ms);
        }
    }

    return changed;
}
