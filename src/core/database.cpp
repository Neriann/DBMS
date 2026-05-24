#include "core/database.hpp"
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace {
    std::filesystem::path make_runtime_path(const std::string &name) {
        static std::size_t counter = 0;
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        auto path = std::filesystem::temp_directory_path() / "dbms_bsp_tree_indexes";
        path /= "database_" + name + "_" + std::to_string(stamp) + "_" + std::to_string(++counter) + ".idx";
        std::filesystem::create_directories(path.parent_path());
        return path;
    }
}

Database::Database(std::string name) : Database(std::move(name), default_index_path(name)) {
}

Database::Database(std::string name, std::filesystem::path index_path)
    : name_(std::move(name)),
      index_path_(std::move(index_path)),
      table_indexes_dir_(index_path_.parent_path() / (index_path_.stem().string() + "_tables")),
      tables_(index_path_) {
    std::filesystem::create_directories(table_indexes_dir_);
}

std::filesystem::path Database::default_index_path(const std::string &name) {
    return make_runtime_path(name);
}

const std::string &Database::name() const noexcept {
    return name_;
}

void Database::create_table(const std::string &table_name, Schema schema) {
    if (tables_.contains(table_name)) {
        throw std::runtime_error("Table '" + table_name + "' already exists");
    }

    const auto index = table_storage_.size();
    const auto indexes_dir = table_indexes_dir_ / table_name;
    table_storage_.push_back(std::make_unique<Table>(std::move(schema), indexes_dir));
    tables_.insert({table_name, index});
}

void Database::drop_table(const std::string &table_name) {
    const auto it = tables_.find(table_name);

    if (it == tables_.end()) {
        throw std::out_of_range("Table '" + table_name + "' does not exist");
    }
    table_storage_.at(it->second).reset();
    tables_.erase(it);
}

Table &Database::get_table(const std::string &table_name) {
    const auto it = tables_.find(table_name);

    if (it == tables_.end()) {
        throw std::out_of_range("Table '" + table_name + "' does not exist");
    }

    return *table_storage_.at(it->second);
}

const Table &Database::get_table(const std::string &table_name) const {
    return *table_storage_.at(tables_.at(table_name));
}

bool Database::has_table(const std::string &table_name) const noexcept {
    return tables_.contains(table_name);
}
