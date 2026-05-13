#include "core/database.hpp"
#include <stdexcept>

Database::Database(std::string name) : name_(std::move(name)) {
}

const std::string &Database::name() const noexcept {
    return name_;
}

void Database::create_table(const std::string &table_name, Schema schema) {
    if (tables_.contains(table_name)) {
        throw std::runtime_error("Table '" + table_name + "' already exists");
    }

    tables_.insert({table_name, std::make_unique<Table>(std::move(schema))});
}

void Database::drop_table(const std::string &table_name) {
    const auto it = tables_.find(table_name);

    if (it == tables_.end()) {
        throw std::out_of_range("Table '" + table_name + "' does not exist");
    }
    tables_.erase(it);
}

Table &Database::get_table(const std::string &table_name) {
    const auto it = tables_.find(table_name);

    if (it == tables_.end()) {
        throw std::out_of_range("Table '" + table_name + "' does not exist");
    }

    return *it->second;
}

const Table &Database::get_table(const std::string &table_name) const {
    return *tables_.at(table_name);
}

bool Database::has_table(const std::string &table_name) const noexcept {
    return tables_.contains(table_name);
}
