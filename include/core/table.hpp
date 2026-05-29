#pragma once
#include <filesystem>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "row.hpp"
#include "schema.hpp"

struct Index;

class Table {
public:
    using TimestampMillis = std::int64_t;

    struct RowVersion {
        RowID row_id{};
        TimestampMillis timestamp_ms{};
        Row row;
        bool deleted = false;
    };

    explicit Table(Schema schema);

    Table(Schema schema, std::filesystem::path indexes_dir);

    ~Table();

    /**
     *
     * @return schema of the table
     */
    [[nodiscard]] const Schema &schema() const noexcept { return schema_; }

    /**
     *
     * @return data of the table
     */
    [[nodiscard]] const std::vector<Row> &data() const noexcept { return data_; }

    /**
     *
     * @param id id of row
     * @return true for rows that have been deleted
     */
    [[nodiscard]] bool is_deleted(RowID id) const;

    /**
     * @param column_name column name
     * @return true if the column has an index
     */
    [[nodiscard]] bool has_index(const std::string &column_name) const noexcept;

    /**
     * @param column_name indexed column name
     * @param value lookup key
     * @return RowIDs matching value, or an empty vector if the column is not indexed / no match exists
     */
    [[nodiscard]] std::vector<RowID> find_indexed(const std::string &column_name, const Value &value) const;

    /**
     * @param column_name indexed column name
     * @param lower optional lower bound
     * @param lower_inclusive whether lower bound is inclusive when present
     * @param upper optional upper bound
     * @param upper_inclusive whether upper bound is inclusive when present
     * @return RowIDs in the requested range, or an empty vector if the column is not indexed / no match exists
     */
    [[nodiscard]] std::vector<RowID> range_indexed(
        const std::string &column_name,
        const std::optional<Value> &lower,
        bool lower_inclusive,
        const std::optional<Value> &upper,
        bool upper_inclusive) const;

    /**
     *
     * @param row row
     * @return RowID of appended row, throws std::invalid_argument on constraint violations.
     */
    RowID insert(const Row &row);

    /**
     *
     * @param rows rows to append atomically
     * @return RowIDs of appended rows, throws std::invalid_argument on constraint violations.
     */
    std::vector<RowID> insert_many(const std::vector<Row> &rows);

    /**
     *
     * @param id row id
     * @param row replace the row at id, update all affected indexes, throws std::out_of_range / std::invalid_argument
     */
    void update(RowID id, Row row);

    /**
     *
     * @param updates row replacements to apply atomically
     */
    void update_many(std::vector<std::pair<RowID, Row> > updates);

    /**
     *
     * @param id - row to be deleted
     */
    void erase(RowID id);

    /**
     *
     * @param timestamp_ms restore rows to the latest version not newer than timestamp_ms
     * @return number of row slots whose visible state changed
     */
    std::size_t revert_to(TimestampMillis timestamp_ms);

    /**
     *
     * @return append-only row version journal
     */
    [[nodiscard]] const std::vector<RowVersion> &history() const noexcept { return history_; }

    /**
     *
     * @param history row version journal restored from persistent storage
     */
    void restore_history(std::vector<RowVersion> history);

private:
    friend class StorageManager;

    Schema schema_;
    std::filesystem::path indexes_dir_;
    std::vector<Row> data_;
    std::vector<bool> deleted_; // tombstone flags
    std::vector<RowVersion> history_;

    std::unordered_map<std::string, std::unique_ptr<Index> > indexes_;

    // region helpers declaration
    /**
     *
     * @param row row that need to be validated
     */
    void validate_row(const Row &row) const;

    /**
     *
     * @param row row restored from persistent storage
     * @param deleted whether the restored slot is a tombstone
     */
    void restore_row(Row row, bool deleted);

    void append_version(RowID id, const Row &row, bool deleted, TimestampMillis timestamp_ms);

    void rebuild_indexes();

    static TimestampMillis current_time_ms();

    // endregion helpers
};
