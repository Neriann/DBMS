#include "storage/storage_manager.hpp"
#include "core/string_interner.hpp"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

using json = nlohmann::json;

namespace {
    json value_to_schema_json(const Value &value) {
        if (std::holds_alternative<int>(value)) {
            return std::get<int>(value);
        }
        if (std::holds_alternative<std::string>(value)) {
            return std::get<std::string>(value);
        }
        return nullptr;
    }

    Value value_from_schema_json(const json &j, const ColumnType type) {
        if (j.is_null()) {
            return nullptr;
        }
        if (type == ColumnType::INT) {
            return j.get<int>();
        }
        if (type == ColumnType::STRING) {
            return j.get<std::string>();
        }

        throw std::runtime_error("Corrupted schema: unknown column type");
    }
}

void to_json(json &j, const Column &c) {
    j = json{
        {"name", c.name},
        {"type", static_cast<int>(c.type)},
        {"constraints", c.constraints}
    };
    if (c.default_value) {
        j["has_default"] = true;
        j["default"] = value_to_schema_json(*c.default_value);
    }
}

void from_json(const json &j, Column &c) {
    j.at("name").get_to(c.name);
    int type_val;
    j.at("type").get_to(type_val);
    c.type = static_cast<ColumnType>(type_val);
    j.at("constraints").get_to(c.constraints);

    bool has_default = false;
    if (j.contains("has_default")) {
        j.at("has_default").get_to(has_default);
    } else {
        has_default = j.contains("default");
    }

    if (has_default) {
        c.default_value = value_from_schema_json(j.at("default"), c.type);
    } else {
        c.default_value.reset();
    }
}

namespace { // visible only here
    void read_safe(std::ifstream &in, char *data, const std::streamsize size) {
        if (!in.read(data, size)) throw std::runtime_error("Corrupted table data: unexpected end of file");
    }

    void write_value(std::ofstream &out, const Value &v) {
        const char type_idx = static_cast<char>(v.index());
        out.write(&type_idx, sizeof(type_idx));

        if (std::holds_alternative<int>(v)) {
            const auto val = std::get<int>(v);
            out.write(reinterpret_cast<const char *>(&val), sizeof(val));
        } else if (std::holds_alternative<InternedString>(v)) {
            const auto &val = std::get<InternedString>(v);
            const std::size_t len = val->size();
            out.write(reinterpret_cast<const char *>(&len), sizeof(len));
            out.write(val->data(), static_cast<std::streamsize>(len));
        }
    }

    Value read_value(std::ifstream &in) {
        char type_idx{};
        read_safe(in, &type_idx, sizeof(type_idx));

        if (type_idx == 0) {
            int val{};
            read_safe(in, reinterpret_cast<char *>(&val), sizeof(val));
            return val;
        }
        if (type_idx == 1) {
            std::size_t len{};
            read_safe(in, reinterpret_cast<char *>(&len), sizeof(len));
            std::string val(len, '\0');
            read_safe(in, val.data(), static_cast<std::streamsize>(len));
            return StringInterner::instance().intern(val);
        }
        if (type_idx == 2) {
            return nullptr;
        }

        throw std::runtime_error("Corrupted table data: unknown value type");
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

void StorageManager::load(DBMS &dbms) const {
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
            auto schema = it.value().get<Schema>();

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

void StorageManager::save(const DBMS &dbms) const {
    for (const auto &entry: fs::directory_iterator(data_dir_)) {
        if (!entry.is_directory()) {
            continue;
        }

        const auto db_name = entry.path().filename().string();
        if (!dbms.has_database(db_name)) {
            fs::remove_all(entry.path());
        }
    }

    for (auto db_it = dbms.databases_.begin(); db_it != dbms.databases_.end(); ++db_it) {
        const auto &db = *db_it->second;

        save_database(db);
        write_schema(db);

        for (auto table_it = db.tables_.begin(); table_it != db.tables_.end(); ++table_it) {
            const auto &table_name = table_it->first;
            const auto &table = *table_it->second;

            write_table_data(db.name(), table_name, table);
        }
    }
}

void StorageManager::save_database(const Database &db) const {
    const auto d_path = db_path(db.name());
    if (!fs::exists(d_path)) {
        fs::create_directories(d_path);
    }
}

void StorageManager::save_table(const Database &db, const std::string &table_name, const Table &table) const {
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

    write_table_data(db_name, table_name, table);
}

void StorageManager::drop_database(const std::string &db_name) const {
    fs::remove_all(db_path(db_name));
}

void StorageManager::drop_table(const std::string &db_name, const std::string &table_name) const {
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

// region helpers implementation
void StorageManager::write_schema(const Database &db) const {
    json j;
    for (auto table_it = db.tables_.begin(); table_it != db.tables_.end(); ++table_it) {
        j[table_it->first] = table_it->second->schema();
    }

    std::ofstream out(schema_path(db.name()));
    out << j.dump(4);
}

void StorageManager::write_table_data(const std::string &db_name, const std::string &table_name,
                                      const Table &table) const {
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
// endregion helpers implementation
