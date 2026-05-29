#pragma once
#include "../core/schema.hpp"
#include "../core/row.hpp"
#include "../core/value.hpp"
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <variant>

//region expressions and conditions

enum class ExprKind {
    Literal,
    Column,
    UnaryMinus
};

/**
 * @note Value literal used if kind == Literal, std::string column used if kind == Column,
 *       std::shared_ptr<Expr> operand used if kind == UnaryMinus
 */
struct Expr {
    ExprKind kind;
    Value literal;
    std::string column;
    std::shared_ptr<Expr> operand;
};

enum class CmpOp {
    EQ,
    NEQ,
    LT,
    GT,
    LEQ,
    GEQ
};

struct Predicate {
    Expr lhs;
    CmpOp op;
    Expr rhs;
};

/**
 * @note A BETWEEN predicate: lhs BETWEEN lo AND hi  →  [lo, hi)
 */
struct BetweenPredicate {
    Expr lhs, lo, hi;
};

/**
 * @note lhs LIKE rhs, where rhs evaluates to a regex string
 */
struct LikePredicate {
    Expr lhs;
    Expr rhs;
};

enum class ConditionKind {
    Simple,
    Between,
    Like,
    And,
    Or
};

struct Condition;

struct Condition {
    ConditionKind kind;
    std::optional<Predicate> predicate;
    std::optional<BetweenPredicate> between;
    std::optional<LikePredicate> like;
    std::shared_ptr<Condition> left;
    std::shared_ptr<Condition> right;
};

//endregion expressions and conditions

//region statement nodes

struct CreateDatabaseStmt {
    std::string db_name;
};

struct DropDatabaseStmt {
    std::string db_name;
};

struct UseStmt {
    std::string db_name;
};

struct ColumnAttributes {
    std::uint8_t constraints = NONE;
    std::optional<Value> default_value;
};

struct CreateTableStmt {
    std::string db_name;
    std::string table_name;
    Schema schema;
};

struct DropTableStmt {
    std::string db_name;
    std::string table_name;
};

enum class AggregateFunction {
    Sum,
    Count,
    Avg
};

struct SelectColumn {
    std::string name;
    std::string alias;
};

struct AggregateCall {
    AggregateFunction function;
    std::string column;
    bool count_star = false;
    std::string alias;
};

using SelectItem = std::variant<SelectColumn, AggregateCall>;

struct InsertStmt {
    std::string db_name;
    std::string table_name;
    std::vector<std::string> columns;
    std::vector<Row> rows;
};

struct UpdateStmt {
    std::string db_name;
    std::string table_name;
    std::vector<std::pair<std::string, Expr> > assignments;
    std::optional<Condition> where;
};

struct DeleteStmt {
    std::string db_name;
    std::string table_name;
    std::optional<Condition> where;
};

struct SelectStmt {
    std::string db_name;
    std::string table_name;
    bool star = false;
    std::vector<SelectItem> items;
    std::optional<Condition> where;
};

//endregion statement nodes

using Statement = std::variant<
    CreateDatabaseStmt, DropDatabaseStmt, UseStmt,
    CreateTableStmt, DropTableStmt,
    InsertStmt, UpdateStmt, DeleteStmt, SelectStmt
>;
