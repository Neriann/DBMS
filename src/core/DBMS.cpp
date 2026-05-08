#include "core/DBMS.hpp"
#include "not_implemented.h"

DBMS::DBMS()
#if DBMS_ALLOCATOR == DBMS_ALLOC_GLOBAL_HEAP
    : databases_alloc_(),
#else
    : databases_alloc_(databases_pool_size_),
#endif
      databases_(pp_allocator<DbTree::value_type>(&databases_alloc_))
{}

void DBMS::create_database(const std::string &/*db_name*/) {
    throw not_implemented("void DBMS::create_database(const std::string &)", "is not implemented");
}

void DBMS::drop_database(const std::string &/*db_name*/) {
    throw not_implemented("void DBMS::drop_database(const std::string &)", "is not implemented");
}

Database &DBMS::get_database(const std::string &/*db_name*/) {
    throw not_implemented("Database &DBMS::get_database(const std::string &)", "is not implemented");
}

const Database &DBMS::get_database(const std::string &/*db_name*/) const {
    throw not_implemented("const Database &DBMS::get_database(const std::string &) const", "is not implemented");
}

bool DBMS::has_database(const std::string &/*db_name*/) const {
    throw not_implemented("bool DBMS::has_database(const std::string &) const", "is not implemented");
}

void DBMS::use(const std::string &/*db_name*/) {
    throw not_implemented("void DBMS::use(const std::string &)", "is not implemented");
}

Database *DBMS::current_database() noexcept {
    return current_db_;
}

const Database *DBMS::current_database() const noexcept {
    return current_db_;
}
