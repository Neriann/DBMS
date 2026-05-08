#pragma once
#include "../core/dbms.hpp"
#include <filesystem>
#include <string>

class StorageManager {
public:
    explicit StorageManager(std::filesystem::path data_dir);

    /**
     * @note load the entire persisted state into `dbms`
     * @param dbms ref to dbms
     */
    void load(DBMS &dbms);

    /**
     * @note persist the entire DBMS state to disk
     * @param dbms ref to dbms
     */
    void save(const DBMS &dbms);

    /**
     * @note persist a single database
     * @param db ref to dbms
     */
    void save_database(const Database &db);

    /**
     *
     * @param db database
     * @param table table that need to be persisted
     * @note persist a single table
     */
    void save_table(const Database &db, const Table &table);

    /**
     *
     * @param db_name database to remove
     * @note remove on-disk artifacts for a dropped database
     */
    void drop_database(const std::string &db_name);

    /**
     *
     * @param db_name database with table to remove
     * @param table_name table to remove
     * @note remove on-disk artifacts for a dropped table
     */
    void drop_table(const std::string &db_name, const std::string &table_name);

private:
    std::filesystem::path data_dir_;

    std::filesystem::path db_path(const std::string &db_name) const;

    std::filesystem::path table_path(const std::string &db_name, const std::string &table_name) const;

    std::filesystem::path schema_path(const std::string &db_name) const;
};
