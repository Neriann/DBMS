#pragma once
#include "database.hpp"
#include "allocator.hpp"
#include <memory>
#include <string>

using DbTree = IndexTree<std::string, std::shared_ptr<Database> >;
using DbAllocator = Allocator;

class DBMS {
    friend class StorageManager;
public:
    DBMS();

    /**
     *
     * @param db_name name of database
     * @note create a database, throws if it already exists.
     */
    void create_database(const std::string &db_name);

    /**
     *
     * @param db_name name of database
     * @note drop a database, throws if it does not exist.
     */
    void drop_database(const std::string &db_name);

    /**
     *
     * @param db_name database name
     * @return returns a reference to the named database or throws std::out_of_range.
     */
    Database &get_database(const std::string &db_name);

    /**
     *
     * @param db_name database name
     * @return returns a const reference to the named database or throws std::out_of_range.
     */
    const Database &get_database(const std::string &db_name) const;

    /**
     *
     * @param db_name database name
     * @return true if table with database name exists else false
     */
    bool has_database(const std::string &db_name) const;

    /**
     *
     * @param db_name database name
     * @note make the db_name current database
     */
    void use(const std::string &db_name);

    /**
     *
     * @return pointer to current database
     */
    Database *current_database() noexcept;

    /**
     *
     * @return const pointer to current database
     */
    const Database *current_database() const noexcept;

private:
    DbAllocator databases_alloc_;
    DbTree databases_;

    Database *current_db_ = nullptr;
};
