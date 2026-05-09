#pragma once
#include <cstdint>
#include <string>
#include <vector>

enum class ColumnType : uint8_t {
    INT,
    STRING
};

enum ConstraintFlags : uint8_t {
    NONE,
    NOT_NULL,
    INDEXED
};

struct Column {
    std::string name;
    ColumnType type;
    uint8_t constraints = NONE;

    /**
     *
     * @return true if column is not null else false
     */
    bool is_not_null() const noexcept;

    /**
     *
     * @return true if column is indexed else false
     */
    bool is_indexed() const noexcept;
};

using Schema = std::vector<Column>;

/**
 *
 * @param schema schema
 * @param name name of the column
 * @return returns the column index or -1 if absent.
 */
int find(const Schema &schema, const std::string &name);
