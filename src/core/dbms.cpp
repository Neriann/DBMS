#include "core/dbms.hpp"
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace {
    std::filesystem::path make_runtime_path(const char *name) {
        static std::size_t counter = 0;
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        auto path = std::filesystem::temp_directory_path() / "dbms_bsp_tree_indexes";
        path /= std::string(name) + "_" + std::to_string(stamp) + "_" + std::to_string(++counter) + ".idx";
        std::filesystem::create_directories(path.parent_path());
        return path;
    }
}

DBMS::DBMS() : DBMS(default_index_path()) {
}

DBMS::DBMS(std::filesystem::path index_path)
    : index_path_(std::move(index_path)),
      database_indexes_dir_(index_path_.parent_path() / (index_path_.stem().string() + "_databases")),
      databases_(index_path_) {
    std::filesystem::create_directories(database_indexes_dir_);
}

std::filesystem::path DBMS::default_index_path() {
    return make_runtime_path("dbms");
}

void DBMS::create_database(const std::string &db_name) {
    if (databases_.contains(db_name)) {
        throw std::runtime_error("Database '" + db_name + "' already exists");
    }

    const auto index = database_storage_.size();
    const auto db_index_path = database_indexes_dir_ / (db_name + ".idx");
    database_storage_.push_back(std::make_unique<Database>(db_name, db_index_path));
    databases_.insert({db_name, index});
}

void DBMS::drop_database(const std::string &db_name) {
    const auto it = databases_.find(db_name);

    if (it == databases_.end()) {
        throw std::out_of_range("Database '" + db_name + "' does not exist");
    }

    auto &slot = database_storage_.at(it->second);
    if (current_db_ == slot.get()) {
        current_db_ = nullptr;
    }
    slot.reset();
    databases_.erase(it);
}

Database &DBMS::get_database(const std::string &db_name) {
    return *database_storage_.at(databases_.at(db_name));
}

const Database &DBMS::get_database(const std::string &db_name) const {
    return *database_storage_.at(databases_.at(db_name));
}

bool DBMS::has_database(const std::string &db_name) const {
    return databases_.contains(db_name);
}

void DBMS::use(const std::string &db_name) {
    const auto it = databases_.find(db_name);

    if (it == databases_.end()) {
        throw std::out_of_range("Database '" + db_name + "' does not exist");
    }
    current_db_ = database_storage_.at(it->second).get();
}

Database *DBMS::current_database() noexcept {
    return current_db_;
}

const Database *DBMS::current_database() const noexcept {
    return current_db_;
}
