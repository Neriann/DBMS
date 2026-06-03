#pragma once
#include "ast.hpp"
#include "../core/dbms.hpp"
#include <string>
#include <unordered_map>


class Executor {
public:
    explicit Executor(DBMS &dbms);

    /**
     * Dispatches a parsed statement to the corresponding executor method.
     *
     * @param stmt parsed statement
     * @return JSON result string; empty string if the statement has no user-visible output
     */
    std::string execute(const Statement &stmt);

private:
    DBMS &dbms_;

    /**
     * @param s CreateDatabaseStmt
     * @return JSON confirmation string
     */
    std::string exec_create_database(const CreateDatabaseStmt &s) const;

    /**
     * @param s DropDatabaseStmt
     * @return JSON confirmation string
     */
    std::string exec_drop_database(const DropDatabaseStmt &s) const;

    /**
     * @param s UseStmt
     * @return JSON confirmation string
     */
    std::string exec_use(const UseStmt &s) const;

    /**
     * @param s CreateTableStmt
     * @return JSON confirmation string
     */
    std::string exec_create_table(const CreateTableStmt &s);

    /**
     * @param s DropTableStmt
     * @return JSON confirmation string
     */
    std::string exec_drop_table(const DropTableStmt &s);

    /**
     * @param s InsertStmt
     * @return JSON confirmation string with number of inserted rows
     */
    std::string exec_insert(const InsertStmt &s);

    /**
     * @param s UpdateStmt
     * @return JSON confirmation string with number of updated rows
     */
    std::string exec_update(const UpdateStmt &s);

    /**
     * @param s DeleteStmt
     * @return JSON confirmation string with number of deleted rows
     */
    std::string exec_delete(const DeleteStmt &s);

    /**
     * @param s SelectStmt
     * @return JSON array of matching rows
     */
    std::string exec_select(const SelectStmt &s);

    /**
     * @param s RevertStmt
     * @return JSON confirmation string
     */
    std::string exec_revert(const RevertStmt &s);

    //region helpers

    /**
     * @param db_name database name
     * @return reference to the Database object
     * @throws std::runtime_error if the database does not exist
     */
    Database &resolve_db(const std::string &db_name);

    /**
     * @param db_name database name
     * @param table_name table name
     * @return reference to the Table object
     * @throws std::runtime_error if the database or table does not exist
     */
    Table &resolve_table(const std::string &db_name, const std::string &table_name);

    /**
     * @param cond condition tree to evaluate
     * @param row row to test
     * @param column_indexes column name to row index lookup
     * @return true if the row satisfies the condition
     */
    static bool eval_condition(
        const Condition &cond,
        const Row &row,
        const std::unordered_map<std::string, int> &column_indexes);

    /**
     * @param expr expression to evaluate
     * @param row row providing column values
     * @param column_indexes column name to row index lookup
     * @return resulting Value
     */
    static Value eval_expr(
        const Expr &expr,
        const Row &row,
        const std::unordered_map<std::string, int> &column_indexes);

    /**
     * @param rows result set to serialise
     * @param col_names column names used as JSON object keys
     * @return JSON array string
     */
    static std::string rows_to_json(const std::vector<Row> &rows, const std::vector<std::string> &col_names);

    //endregion helpers
};
