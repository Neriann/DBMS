#include "storage/storage_manager.hpp"
#include "core/table.hpp"
#include "core/dbms.hpp"
#include "core/database.hpp"
#include "value_test_utils.hpp"
#include <gtest/gtest.h>

TEST(StorageManager, LoadSave) {
    auto test_dir = "test_data";
    std::filesystem::remove_all(test_dir);
    {
        DBMS dbms;
        dbms.create_database("db1");
        dbms.use("db1");
        auto *db = dbms.current_database();
        Schema s = {{"col1", ColumnType::INT, NONE}, {"col2", ColumnType::STRING, NONE}};
        db->create_table("t1", s);

        auto &t1 = db->get_table("t1");
        t1.insert({42, interned_value("hello")});
        t1.insert({100, interned_value("world")});

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
        EXPECT_EQ(interned_string(t1_load.data()[0][1]), "hello");
        EXPECT_EQ(std::get<int>(t1_load.data()[1][0]), 100);
        EXPECT_EQ(interned_string(t1_load.data()[1][1]), "world");
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
        Schema s = {{"id", ColumnType::INT, INDEXED}, {"name", ColumnType::STRING, NONE}};
        db->create_table("t1", s);

        auto &t1 = db->get_table("t1");
        t1.insert({1, interned_value("deleted")});
        t1.insert({2, interned_value("kept")});
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

        EXPECT_THROW(t1_load.update(0, {1, interned_value("still deleted")}), std::out_of_range);
        EXPECT_NO_THROW(t1_load.update(1, {3, interned_value("updated")}));
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
        db->create_table("users", {{"id", ColumnType::INT, INDEXED}, {"name", ColumnType::STRING, NONE}});
        db->create_table("notes", {{"body", ColumnType::STRING, NONE}, {"rank", ColumnType::INT, NONE}});

        db->get_table("users").insert({1, interned_value("Sebastian")});
        db->get_table("notes").insert({interned_value("hello"), 10});

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
        EXPECT_EQ(interned_string(users.data()[0][1]), "Sebastian");

        ASSERT_EQ(notes.data().size(), 1);
        EXPECT_EQ(interned_string(notes.data()[0][0]), "hello");
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

        const StorageManager sm(test_dir);
        sm.save(dbms);

        dbms.drop_database("dropped");
        sm.save(dbms);
    }

    {
        DBMS dbms2;
        const StorageManager sm(test_dir);
        sm.load(dbms2);

        EXPECT_TRUE(dbms2.has_database("kept"));
        EXPECT_FALSE(dbms2.has_database("dropped"));
        EXPECT_FALSE(std::filesystem::exists(std::filesystem::path(test_dir) / "dropped"));
    }
}

TEST(Table, InsertManyDuplicateIndexedValues) {
    Table table({{"id", ColumnType::INT, INDEXED}, {"name", ColumnType::STRING, NONE}});

    EXPECT_THROW(
        {
        table.insert_many(std::vector<Row>{
            {1, interned_value("Ann")}
            ,{1, interned_value("Duplicate")}
            });
        },
        std::invalid_argument
    );

    EXPECT_TRUE(table.data().empty());
}

TEST(Table, UpdateManyDuplicateFinalIndexedValues) {
    Table table({{"id", ColumnType::INT, INDEXED}, {"name", ColumnType::STRING, NONE}});
    table.insert_many({
        {1, interned_value("Ann")},
        {2, interned_value("Bob")}
    });

    EXPECT_THROW(
        {
        table.update_many(std::vector<std::pair<RowID, Row> >{
            {0, {3, interned_value("Ann")}},
            {1, {3, interned_value("Bob")}}
            });
        },
        std::invalid_argument
    );

    ASSERT_EQ(table.data().size(), 2);
    EXPECT_EQ(std::get<int>(table.data()[0][0]), 1);
    EXPECT_EQ(std::get<int>(table.data()[1][0]), 2);
}

TEST(Table, UpdateManyAllowsIndexedValueSwap) {
    Table table({{"id", ColumnType::INT, INDEXED}, {"name", ColumnType::STRING, NONE}});
    table.insert_many({
        {1, interned_value("Ann")},
        {2, interned_value("Bob")}
    });

    EXPECT_NO_THROW(
        {
        table.update_many(std::vector<std::pair<RowID, Row> >{
            {0, {2, interned_value("Ann")}},
            {1, {1, interned_value("Bob")}}
            });
        }
    );

    EXPECT_EQ(std::get<int>(table.data()[0][0]), 2);
    EXPECT_EQ(std::get<int>(table.data()[1][0]), 1);
}
