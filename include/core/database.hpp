#pragma once
#include "table.hpp"
#include <memory>
#include <string>

#include "index_tree.hpp"

class Database {
public:
    explicit Database(std::string name);

    /**
     *
     * @return name of the database
     */
    const std::string &name() const noexcept;

    /**
     *
     * @param table_name table name
     * @param schema schema
     * @note create a new table, throws if a table with that name already exists.
     */
    void create_table(const std::string &table_name, Schema schema);

    /**
     *
     * @param table_name table name
     * @note drops a table, throws if it does not exist.
     */
    void drop_table(const std::string &table_name);

    /**
     *
     * @param table_name table name
     * @return table or throws std::out_of_range.
     */
    Table &get_table(const std::string &table_name);

    /**
     *
     * @param table_name table name
     * @return const table or throws std::out_of_range.
     */
    const Table &get_table(const std::string &table_name) const;

    /**
     *
     * @param table_name table name
     * @return true if table with table_name exists else false
     */
    bool has_table(const std::string &table_name) const noexcept;

private:
    std::string name_;

    IndexTree<std::string, std::unique_ptr<Table> > tables_;
};
