#pragma once
#include "Database.hpp"
#include <memory>
#include <string>

#if DBMS_INDEX_TREE == DBMS_TREE_B
#include <b_tree.h>
using DbTree = B_tree<std::string, std::unique_ptr<Database> >;
#elif DBMS_INDEX_TREE == DBMS_TREE_BP
#include <b_plus_tree.h>
using DbTree = BP_tree<std::string, std::unique_ptr<Database> >;
#elif DBMS_INDEX_TREE == DBMS_TREE_BS
#include <b_star_tree.h>
using DbTree = BS_tree<std::string, std::unique_ptr<Database> >;
#else
#include <b_star_plus_tree.h>
using DbTree = BSP_tree<std::string, std::unique_ptr<Database> >;
#endif

#if DBMS_ALLOCATOR == DBMS_ALLOC_GLOBAL_HEAP
#include <allocator_global_heap.h>
using DbAllocator = allocator_global_heap;
#elif DBMS_ALLOCATOR == DBMS_ALLOC_BOUNDARY_TAGS
#include <allocator_boundary_tags.h>
using DbAllocator = allocator_boundary_tags;
#elif DBMS_ALLOCATOR == DBMS_ALLOC_BUDDIES
#include <allocator_buddies_system.h>
using DbAllocator = allocator_buddies_system;
#elif DBMS_ALLOCATOR == DBMS_ALLOC_SORTED_LIST
#include <allocator_sorted_list.h>
using DbAllocator = allocator_sorted_list;
#else
#include <allocator_red_black_tree.h>
using DbAllocator = allocator_red_black_tree;
#endif

class DBMS {
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
    // TODO: remove hardcoded size;
    static constexpr size_t databases_pool_size_ = 1u << 20; // 1 MiB

    DbAllocator databases_alloc_;
    DbTree databases_;

    Database *current_db_ = nullptr;
};
