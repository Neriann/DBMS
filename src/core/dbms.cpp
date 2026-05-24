#include "core/dbms.hpp"
#include <stdexcept>

DBMS::DBMS()
    : databases_(pp_allocator<DbTree::value_type>(&databases_alloc_)) {
}

void DBMS::create_database(const std::string &db_name) {
    if (databases_.contains(db_name)) {
        throw std::runtime_error("Database '" + db_name + "' already exists");
    }

    databases_.insert({db_name, std::make_unique<Database>(db_name)});
}

void DBMS::drop_database(const std::string &db_name) {
    const auto it = databases_.find(db_name);

    if (it == databases_.end()) {
        throw std::out_of_range("Database '" + db_name + "' does not exist");
    }

    if (current_db_ == it->second.get()) {
        current_db_ = nullptr;
    }
    databases_.erase(it);
}

Database &DBMS::get_database(const std::string &db_name) {
    return *databases_.at(db_name);
}

const Database &DBMS::get_database(const std::string &db_name) const {
    return *databases_.at(db_name);
}

bool DBMS::has_database(const std::string &db_name) const {
    return databases_.contains(db_name);
}

void DBMS::use(const std::string &db_name) {
    const auto it = databases_.find(db_name);

    if (it == databases_.end()) {
        throw std::out_of_range("Database '" + db_name + "' does not exist");
    }
    current_db_ = it->second.get();
}

Database *DBMS::current_database() noexcept {
    return current_db_;
}

const Database *DBMS::current_database() const noexcept {
    return current_db_;
}
