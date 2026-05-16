#include "storage/storage_manager.hpp"
#include "core/dbms.hpp"
#include "core/database.hpp"
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
        t1.insert({42, std::string("hello")});
        t1.insert({100, std::string("world")});

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
        EXPECT_EQ(std::get<std::string>(t1_load.data()[0][1]), "hello");
        EXPECT_EQ(std::get<int>(t1_load.data()[1][0]), 100);
        EXPECT_EQ(std::get<std::string>(t1_load.data()[1][1]), "world");
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
        t1.insert({1, std::string("deleted")});
        t1.insert({2, std::string("kept")});
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

        EXPECT_THROW(t1_load.update(0, {1, std::string("still deleted")}), std::out_of_range);
        EXPECT_NO_THROW(t1_load.update(1, {3, std::string("updated")}));
    }
}
