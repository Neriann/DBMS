#pragma once
#include "../core/dbms.hpp"
#include <filesystem>
#include <string>
#include "core/string_interner.hpp"

namespace fs = std::filesystem;

class StorageManager {
public:
    explicit StorageManager(fs::path data_dir);

    /**
     * @note load the entire persisted state into `dbms`
     * @param dbms ref to dbms
     */
    void load(DBMS &dbms) const;

    /**
     * @note persist the entire DBMS state to disk
     * @param dbms ref to dbms
     */
    void save(const DBMS &dbms) const;

    /**
     * @note persist a single database
     * @param db ref to database
     */
    void save_database(const Database &db) const;

    /**
     *
     * @param db database
     * @param table_name table name
     * @param table table that need to be persisted
     * @note persist a single table
     */
    void save_table(const Database &db, const std::string &table_name, const Table &table) const;

    /**
     *
     * @param db_name database to remove
     * @note remove on-disk artifacts for a dropped database
     */
    void drop_database(const std::string &db_name) const;

    /**
     *
     * @param db_name database with table to remove
     * @param table_name table to remove
     * @note remove on-disk artifacts for a dropped table
     */
    void drop_table(const std::string &db_name, const std::string &table_name) const;

private:
    fs::path data_dir_;

    [[nodiscard]] fs::path db_path(const std::string &db_name) const;

    [[nodiscard]] fs::path table_path(const std::string &db_name, const std::string &table_name) const;

    [[nodiscard]] fs::path schema_path(const std::string &db_name) const;

    // region helpers declaration
    void write_schema(const Database &db) const;

    void write_table_data(const std::string &db_name, const std::string &table_name, const Table &table) const;
    // endregion helpers declaration
};
