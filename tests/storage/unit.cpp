#include "storage/storage_manager.hpp"
#include "core/dbms.hpp"
#include "core/database.hpp"
#include "core/row.hpp"
#include <gtest/gtest.h>

TEST(StorageManager, LoadSave) {
    std::string test_dir = "test_data";
    std::filesystem::remove_all(test_dir);
    {
        DBMS dbms;
        dbms.create_database("db1");
        dbms.use("db1");
        Database* db = dbms.current_database();
        Schema s = {{"col1", ColumnType::INT, NONE}, {"col2", ColumnType::STRING, NONE}};
        db->create_table("t1", s);

        Table& t1 = db->get_table("t1");
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
        const Table& t1_load = dbms2.current_database()->get_table("t1");

        ASSERT_EQ(t1_load.data().size(), 2);
        EXPECT_EQ(std::get<int>(t1_load.data()[0][0]), 42);
        EXPECT_EQ(std::get<std::string>(t1_load.data()[0][1]), "hello");
        EXPECT_EQ(std::get<int>(t1_load.data()[1][0]), 100);
        EXPECT_EQ(std::get<std::string>(t1_load.data()[1][1]), "world");
    }
}
