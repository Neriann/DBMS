#include "storage/storage_manager.hpp"
#include "core/table.hpp"
#include "core/dbms.hpp"
#include "core/database.hpp"
#include <gtest/gtest.h>
#include <filesystem>

static InternedString S(const std::string& s) {
    return InternedString{global_string_pool().intern(s)};
}

TEST(StorageManager, LoadSave) {
    auto test_dir = "test_data";
    std::filesystem::remove_all(test_dir);

    {
        DBMS dbms;
        dbms.create_database("db1");
        dbms.use("db1");

        auto *db = dbms.current_database();

        Schema s = {
            {"col1", ColumnType::INT, NONE},
            {"col2", ColumnType::STRING, NONE}
        };

        db->create_table("t1", s);

        auto &t1 = db->get_table("t1");

        t1.insert({42, S("hello")});
        t1.insert({100, S("world")});

        StorageManager sm(test_dir);
        sm.save_database(*db);
        sm.save_table(*db, "t1", t1);
    }

    {
        DBMS dbms2;
        StorageManager sm(test_dir);
        sm.load(dbms2);

        dbms2.use("db1");
        const auto &t1_load = dbms2.current_database()->get_table("t1");

        ASSERT_EQ(t1_load.data().size(), 2);
        EXPECT_EQ(std::get<int>(t1_load.data()[0][0]), 42);
        const InternedString& is1 = std::get<InternedString>(t1_load.data()[0][1]);
        const std::string& str1 = global_string_pool().get(is1.id);
        EXPECT_EQ(str1, "hello");

        EXPECT_EQ(std::get<int>(t1_load.data()[1][0]), 100);
        const InternedString& is2 = std::get<InternedString>(t1_load.data()[1][1]);
        const std::string& str2 = global_string_pool().get(is2.id);
        EXPECT_EQ(str2, "world");
    }
}

TEST(StorageManager, LoadWithDeletedRows) {
    auto test_dir = "test_data_deleted_rows";
    std::filesystem::remove_all(test_dir);

    {
        DBMS dbms;
        dbms.create_database("db1");
        dbms.use("db1");

        auto *db = dbms.current_database();

        Schema s = {
            {"id", ColumnType::INT, INDEXED},
            {"name", ColumnType::STRING, NONE}
        };

        db->create_table("t1", s);

        auto &t1 = db->get_table("t1");

        t1.insert({1, S("deleted")});
        t1.insert({2, S("kept")});
        t1.erase(0);

        StorageManager sm(test_dir);
        sm.save_database(*db);
        sm.save_table(*db, "t1", t1);
    }

    {
        DBMS dbms2;
        StorageManager sm(test_dir);
        sm.load(dbms2);

        dbms2.use("db1");
        auto &t1_load = dbms2.current_database()->get_table("t1");

        ASSERT_EQ(t1_load.data().size(), 2);

        EXPECT_TRUE(t1_load.is_deleted(0));
        EXPECT_FALSE(t1_load.is_deleted(1));

        EXPECT_EQ(std::get<int>(t1_load.data()[1][0]), 2);

        EXPECT_THROW(
            t1_load.update(0, {1, S("still deleted")}),
            std::out_of_range
        );

        EXPECT_NO_THROW(
            t1_load.update(1, {3, S("updated")})
        );
    }
}

TEST(StorageManager, SaveEntireDbms) {
    auto test_dir = "test_data_full_save";
    std::filesystem::remove_all(test_dir);

    {
        DBMS dbms;
        dbms.create_database("db1");
        dbms.use("db1");

        auto *db = dbms.current_database();

        db->create_table("users",
            {{"id", ColumnType::INT, INDEXED},
             {"name", ColumnType::STRING, NONE}});

        db->create_table("notes",
            {{"body", ColumnType::STRING, NONE},
             {"rank", ColumnType::INT, NONE}});

        db->get_table("users").insert({1, S("Sebastian")});
        db->get_table("notes").insert({S("hello"), 10});

        StorageManager sm(test_dir);
        sm.save(dbms);
    }

    {
        DBMS dbms2;
        StorageManager sm(test_dir);
        sm.load(dbms2);

        dbms2.use("db1");
        auto *db = dbms2.current_database();

        const auto &users = db->get_table("users");
        const auto &notes = db->get_table("notes");

        ASSERT_EQ(users.data().size(), 1);
        EXPECT_EQ(std::get<int>(users.data()[0][0]), 1);
        const InternedString& is1 = std::get<InternedString>(users.data()[0][1]);
        const std::string& str1 = global_string_pool().get(is1.id);
        EXPECT_EQ(str1, "Sebastian");

        ASSERT_EQ(notes.data().size(), 1);
        const InternedString& is2 = std::get<InternedString>(notes.data()[0][0]);
        const std::string& str2 = global_string_pool().get(is2.id);
        EXPECT_EQ(str2, "hello");
        EXPECT_EQ(std::get<int>(notes.data()[0][1]), 10);
    }
}

TEST(StorageManager, SavePrunesDroppedDatabases) {
    const auto test_dir = "test_data_prune_dropped_databases";
    std::filesystem::remove_all(test_dir);

    {
        DBMS dbms;
        dbms.create_database("kept");
        dbms.create_database("dropped");

        StorageManager sm(test_dir);
        sm.save(dbms);

        dbms.drop_database("dropped");
        sm.save(dbms);
    }

    {
        DBMS dbms2;
        StorageManager sm(test_dir);
        sm.load(dbms2);

        EXPECT_TRUE(dbms2.has_database("kept"));
        EXPECT_FALSE(dbms2.has_database("dropped"));

        EXPECT_FALSE(
            std::filesystem::exists(
                std::filesystem::path(test_dir) / "dropped"
            )
        );
    }
}

TEST(Table, InsertManyDuplicateIndexedValues) {
    Schema schema = {
        {"id", ColumnType::INT, INDEXED},
        {"name", ColumnType::STRING, NONE}
    };
    Table table(schema, nullptr);

    std::vector<Row> rows = {
        {1, InternedString{global_string_pool().intern("John")}},
        {2, InternedString{global_string_pool().intern("Jane")}}
    };

    EXPECT_THROW(
        table.insert_many(rows),
        std::invalid_argument
    );

    EXPECT_TRUE(table.data().empty());
}

TEST(Table, UpdateManyDuplicateFinalIndexedValues) {
    Schema schema = {
        {"id", ColumnType::INT, INDEXED},
        {"name", ColumnType::STRING, NONE}
    };
    Table table(schema, nullptr);

    table.insert_many({
        {1, InternedString{global_string_pool().intern("Ann")}},
        {2, InternedString{global_string_pool().intern("Bob")}}
    });

    EXPECT_THROW(
        table.update_many({
            {0, {3, InternedString{global_string_pool().intern("Ann")}}},
            {1, {3, InternedString{global_string_pool().intern("Bob")}}}
        }),
        std::invalid_argument
    );

    ASSERT_EQ(table.data().size(), 2);
    EXPECT_EQ(std::get<int>(table.data()[0][0]), 1);
    EXPECT_EQ(std::get<int>(table.data()[1][0]), 2);
}

TEST(Table, UpdateManyAllowsIndexedValueSwap) {
    Schema schema = {
        {"id", ColumnType::INT, INDEXED},
        {"name", ColumnType::STRING, NONE}
    };
    Table table(schema, nullptr);
    table.insert_many({
        {1, InternedString{global_string_pool().intern("Ann")}},
        {2, InternedString{global_string_pool().intern("Bob")}}
    });

    EXPECT_NO_THROW(
        {
        table.update_many(std::vector<std::pair<RowID, Row> >{
            {0, {2, InternedString{global_string_pool().intern("Ann")}}},
            {1, {1, InternedString{global_string_pool().intern("Bob")}}}
            });
        }
    );

    EXPECT_EQ(std::get<int>(table.data()[0][0]), 2);
    EXPECT_EQ(std::get<int>(table.data()[1][0]), 1);
}
