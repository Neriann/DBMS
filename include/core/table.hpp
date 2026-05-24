#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "row.hpp"
#include "schema.hpp"

struct Index;

class Table {
public:
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

private:
    friend class StorageManager;

    Schema schema_;
    std::filesystem::path indexes_dir_;
    std::vector<Row> data_;
    std::vector<bool> deleted_; // tombstone flags

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

    // endregion helpers
};
