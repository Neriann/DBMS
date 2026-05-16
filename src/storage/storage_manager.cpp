#include "storage/storage_manager.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

void to_json(json &j, const Column &c) {
    j = json{
        {"name", c.name},
        {"type", static_cast<int>(c.type)},
        {"constraints", c.constraints}
    };
}

void from_json(const json &j, Column &c) {
    j.at("name").get_to(c.name);
    int type_val;
    j.at("type").get_to(type_val);
    c.type = static_cast<ColumnType>(type_val);
    j.at("constraints").get_to(c.constraints);
}

namespace {
    // visible only here
    void write_value(std::ofstream &out, const Value &v) {
        const char type_idx = static_cast<char>(v.index());
        out.write(&type_idx, sizeof(type_idx));

        if (std::holds_alternative<int>(v)) {
            const auto val = std::get<int>(v);
            out.write(reinterpret_cast<const char *>(&val), sizeof(val));
        } else if (std::holds_alternative<std::string>(v)) {
            const auto &val = std::get<std::string>(v);
            const std::size_t len = val.size();
            out.write(reinterpret_cast<const char *>(&len), sizeof(len));
            out.write(val.data(), static_cast<std::streamsize>(len));
        }
    }

    Value read_value(std::ifstream &in) {
        char type_idx{};
        in.read(&type_idx, sizeof(type_idx));

        if (type_idx == 0) {
            int val{};
            in.read(reinterpret_cast<char *>(&val), sizeof(val));
            return val;
        }
        if (type_idx == 1) {
            std::size_t len{};
            in.read(reinterpret_cast<char *>(&len), sizeof(len));
            std::string val(len, '\0');
            in.read(val.data(), static_cast<std::streamsize>(len));
            return val;
        }
        return nullptr;
    }
}

StorageManager::StorageManager(std::filesystem::path data_dir)
    : data_dir_(std::move(data_dir)) {
    if (!fs::exists(data_dir_)) {
        fs::create_directories(data_dir_);
    }
}

fs::path StorageManager::db_path(const std::string &db_name) const {
    return data_dir_ / db_name;
}

fs::path StorageManager::schema_path(const std::string &db_name) const {
    return db_path(db_name) / "schema.json";
}

fs::path StorageManager::table_path(const std::string &db_name, const std::string &table_name) const {
    return db_path(db_name) / (table_name + ".bin");
}

void StorageManager::load(DBMS &dbms) {
    if (!fs::exists(data_dir_)) {
        return;
    }

    for (const auto &entry: fs::directory_iterator(data_dir_)) {
        if (!entry.is_directory()) {
            continue;
        }

        const auto db_name = entry.path().filename().string();
        dbms.create_database(db_name);
        auto &db = dbms.get_database(db_name);

        const auto s_path = schema_path(db_name);
        if (!fs::exists(s_path)) {
            continue;
        }

        std::ifstream sdf(s_path);
        json j;
        sdf >> j;

        for (auto it = j.begin(); it != j.end(); ++it) {
            const auto &table_name = it.key();
            Schema schema = it.value().get<Schema>();

            db.create_table(table_name, schema);
            auto &table = db.get_table(table_name);

            const auto t_path = table_path(db_name, table_name);
            if (!fs::exists(t_path)) {
                continue;
            }

            std::ifstream tdf(t_path, std::ios::binary);
            bool deleted{};
            while (tdf.read(reinterpret_cast<char *>(&deleted), sizeof(deleted))) {
                Row row;
                row.reserve(schema.size());
                for (std::size_t i = 0; i < schema.size(); ++i) {
                    row.push_back(read_value(tdf));
                }

                table.restore_row(std::move(row), deleted);
            }
        }
    }
}

void StorageManager::save(const DBMS &dbms) {
    for (auto db_it = dbms.databases_.begin(); db_it != dbms.databases_.end(); ++db_it) {
        const auto &db = *db_it->second;

        save_database(db);

        for (auto table_it = db.tables_.begin(); table_it != db.tables_.end(); ++table_it) {
            const auto &table_name = table_it->first;
            const auto &table = *table_it->second;

            save_table(db, table_name, table);
        }
    }
}

void StorageManager::save_database(const Database &db) {
    const auto d_path = db_path(db.name());
    if (!fs::exists(d_path)) {
        fs::create_directories(d_path);
    }
}

void StorageManager::save_table(const Database &db, const std::string &table_name, const Table &table) {
    const auto db_name = db.name();

    const auto s_path = schema_path(db_name);
    json j;
    if (fs::exists(s_path)) {
        std::ifstream sdf(s_path);
        sdf >> j;
    }

    j[table_name] = table.schema();

    std::ofstream sdf_out(s_path);
    sdf_out << j.dump(4);

    const auto t_path = table_path(db_name, table_name);
    std::ofstream tdf(t_path, std::ios::binary);

    const auto &data = table.data();
    for (RowID id = 0; id < data.size(); ++id) {
        const bool deleted = table.is_deleted(id);
        tdf.write(reinterpret_cast<const char *>(&deleted), sizeof(deleted));

        for (const auto &v: data[id]) {
            write_value(tdf, v);
        }
    }
}

void StorageManager::drop_database(const std::string &db_name) {
    fs::remove_all(db_path(db_name));
}

void StorageManager::drop_table(const std::string &db_name, const std::string &table_name) {
    fs::remove(table_path(db_name, table_name));

    const auto s_path = schema_path(db_name);
    if (fs::exists(s_path)) {
        std::ifstream sdf(s_path);
        json j;
        sdf >> j;
        j.erase(table_name);

        std::ofstream out(s_path);
        out << j.dump(4);
    }
}
