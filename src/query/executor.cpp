#include "query/executor.hpp"
#include "not_implemented.h"

Executor::Executor(DBMS &dbms) : dbms_(dbms) {
}

std::string Executor::execute(const Statement &/*stmt*/) {
    throw not_implemented("std::string Executor::execute(const Statement &)", "is not implemented");
}

Database &Executor::resolve_db(const std::string &/*db_name*/) {
    throw not_implemented("Database &Executor::resolve_db(const std::string &)", "is not implemented");
}

Table &Executor::resolve_table(const std::string &/*db_name*/, const std::string &/*table_name*/) {
    throw not_implemented("Table &Executor::resolve_table(const std::string &, const std::string &)",
                          "is not implemented");
}

std::string Executor::exec_create_database(const CreateDatabaseStmt &/*s*/) const {
    throw not_implemented("std::string Executor::exec_create_database(const CreateDatabaseStmt &) const",
                          "is not implemented");
}

std::string Executor::exec_drop_database(const DropDatabaseStmt &/*s*/) const {
    throw not_implemented("std::string Executor::exec_drop_database(const DropDatabaseStmt &) const",
                          "is not implemented");
}

std::string Executor::exec_use(const UseStmt &/*s*/) const {
    throw not_implemented("std::string Executor::exec_use(const UseStmt &) const", "is not implemented");
}

std::string Executor::exec_create_table(const CreateTableStmt &/*s*/) {
    throw not_implemented("std::string Executor::exec_create_table(const CreateTableStmt &)", "is not implemented");
}

std::string Executor::exec_drop_table(const DropTableStmt &/*s*/) {
    throw not_implemented("std::string Executor::exec_drop_table(const DropTableStmt &)", "is not implemented");
}

std::string Executor::exec_insert(const InsertStmt &/*s*/) {
    throw not_implemented("std::string Executor::exec_insert(const InsertStmt &)", "is not implemented");
}

std::string Executor::exec_update(const UpdateStmt &/*s*/) {
    throw not_implemented("std::string Executor::exec_update(const UpdateStmt &)", "is not implemented");
}

std::string Executor::exec_delete(const DeleteStmt &/*s*/) {
    throw not_implemented("std::string Executor::exec_delete(const DeleteStmt &)", "is not implemented");
}

std::string Executor::exec_select(const SelectStmt &/*s*/) {
    throw not_implemented("std::string Executor::exec_select(const SelectStmt &)", "is not implemented");
}

bool Executor::eval_condition(const Condition &/*cond*/, const Row &/*row*/, const Schema &/*schema*/) {
    throw not_implemented("bool Executor::eval_condition(const Condition &, const Row &, const Schema &)",
                          "is not implemented");
}

Value Executor::eval_expr(const Expr &/*expr*/, const Row &/*row*/, const Schema &/*schema*/) {
    throw not_implemented("Value Executor::eval_expr(const Expr &, const Row &, const Schema &)", "is not implemented");
}

std::string Executor::rows_to_json(const std::vector<Row> &/*rows*/, const std::vector<std::string> &/*col_names*/) {
    throw not_implemented(
        "std::string Executor::rows_to_json(const std::vector<Row> &, const std::vector<std::string> &)",
        "is not implemented");
}
