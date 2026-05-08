#include "storage/storage_manager.hpp"
#include "not_implemented.h"

StorageManager::StorageManager(std::filesystem::path data_dir)
    : data_dir_(std::move(data_dir)) {
}

std::filesystem::path StorageManager::db_path(const std::string &/*db_name*/) const {
    throw not_implemented("std::filesystem::path StorageManager::db_path(const std::string &) const",
                          "is not implemented");
}

std::filesystem::path StorageManager::schema_path(const std::string &/*db_name*/) const {
    throw not_implemented("std::filesystem::path StorageManager::schema_path(const std::string &) const",
                          "is not implemented");
}

std::filesystem::path StorageManager::table_path(const std::string &/*db_name*/,
                                                 const std::string &/*table_name*/) const {
    throw not_implemented(
        "std::filesystem::path StorageManager::table_path(const std::string &, const std::string &) const",
        "is not implemented");
}

void StorageManager::load(DBMS &/*dbms*/) {
    throw not_implemented("void StorageManager::load(DBMS &)", "is not implemented");
}

void StorageManager::save(const DBMS &/*dbms*/) {
    throw not_implemented("void StorageManager::save(const DBMS &)", "is not implemented");
}

void StorageManager::save_database(const Database &/*db*/) {
    throw not_implemented("void StorageManager::save_database(const Database &)", "is not implemented");
}

void StorageManager::save_table(const Database &/*db*/, const Table &/*table*/) {
    throw not_implemented("void StorageManager::save_table(const Database &, const Table &)", "is not implemented");
}

void StorageManager::drop_database(const std::string &/*db_name*/) {
    throw not_implemented("void StorageManager::drop_database(const std::string &)", "is not implemented");
}

void StorageManager::drop_table(const std::string &/*db_name*/, const std::string &/*table_name*/) {
    throw not_implemented("void StorageManager::drop_table(const std::string &, const std::string &)",
                          "is not implemented");
}
