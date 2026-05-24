#pragma once
#include "table.hpp"
#include "trees/b_star_plus_tree.hpp"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class Database {
    friend class StorageManager;
public:
    explicit Database(std::string name);

    Database(std::string name, std::filesystem::path index_path);

    /**
     *
     * @return name of the database
     */
    [[nodiscard]] const std::string &name() const noexcept;

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
    [[nodiscard]] const Table &get_table(const std::string &table_name) const;

    /**
     *
     * @param table_name table name
     * @return true if table with table_name exists else false
     */
    [[nodiscard]] bool has_table(const std::string &table_name) const noexcept;

private:
    static std::filesystem::path default_index_path(const std::string &name);

    std::string name_;
    std::filesystem::path index_path_;
    std::filesystem::path table_indexes_dir_;

    BSP_tree<std::string, std::size_t> tables_;
    std::vector<std::unique_ptr<Table> > table_storage_;
};
