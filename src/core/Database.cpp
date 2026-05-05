#include "core/Database.hpp"
#include "not_implemented.h"

Database::Database(std::string name) : name_(std::move(name)) {
}

const std::string &Database::name() const noexcept {
    return name_;
}

void Database::create_table(const std::string &/*table_name*/, Schema /*schema*/) {
    throw not_implemented("void Database::create_table(const std::string &, Schema)", "is not implemented");
}

void Database::drop_table(const std::string &/*table_name*/) {
    throw not_implemented("void Database::drop_table(const std::string &)", "is not implemented");
}

Table &Database::get_table(const std::string &/*table_name*/) {
    throw not_implemented("Table &Database::get_table(const std::string &)", "is not implemented");
}

const Table &Database::get_table(const std::string &/*table_name*/) const {
    throw not_implemented("const Table &Database::get_table(const std::string &) const", "is not implemented");
}

bool Database::has_table(const std::string &table_name) const noexcept {
    return tables_.contains(table_name);
}
