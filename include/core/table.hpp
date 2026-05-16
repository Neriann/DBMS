#pragma once
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
     * @param id
     * @param row replace the row at `id` and update all affected indexes, throws std::out_of_range / std::invalid_argument as appropriate.
     */
    void update(RowID id, Row row);

    /**
     *
     * @param id - row to be deleted
     */
    void erase(RowID id);

private:
    Schema schema_;
    std::vector<Row> data_;
    std::vector<bool> deleted_; // tombstone flags

    std::unordered_map<std::string, std::unique_ptr<Index> > indexes_;

    // region helpers declaration
    /**
     *
     * @param row row that need to be validated
     */
    void validate_row(const Row &row) const;

    // endregion helpers
};
