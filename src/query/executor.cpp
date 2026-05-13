#include "query/executor.hpp"

#include <nlohmann/json.hpp>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <variant>

namespace {

template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

nlohmann::json ok_json(const std::string &message) {
    return nlohmann::json{{"status", "ok"}, {"message", message}};
}

nlohmann::json count_json(const std::string &operation, const std::size_t count) {
    return nlohmann::json{{"status", "ok"}, {"operation", operation}, {"count", count}};
}

bool is_null(const Value &value) {
    return std::holds_alternative<std::nullptr_t>(value);
}

int require_column(const Schema &schema, const std::string &name) {
    const int index = find(schema, name);
    if (index < 0) {
        throw std::runtime_error("Unknown column '" + name + "'");
    }
    return index;
}

void ensure_no_duplicate(const std::set<std::string> &names, const std::string &name) {
    if (names.contains(name)) {
        throw std::runtime_error("Duplicate column '" + name + "'");
    }
}

int compare_values(const Value &lhs, const Value &rhs) {
    if (lhs.index() != rhs.index()) {
        throw std::runtime_error("Cannot compare values of different types");
    }
    if (std::holds_alternative<std::nullptr_t>(lhs)) {
        throw std::runtime_error("Cannot order NULL values");
    }
    if (std::holds_alternative<int>(lhs)) {
        const int l = std::get<int>(lhs);
        const int r = std::get<int>(rhs);
        return (l > r) - (l < r);
    }

    const std::string &l = std::get<std::string>(lhs);
    const std::string &r = std::get<std::string>(rhs);
    return (l > r) - (l < r);
}

nlohmann::json value_to_json(const Value &value) {
    return std::visit(
        Overloaded{
            [](const int v) -> nlohmann::json { return v; },
            [](const std::string &v) -> nlohmann::json { return v; },
            [](std::nullptr_t) -> nlohmann::json { return nullptr; }
        },
        value);
}

} // namespace

// region Lifecycle

Executor::Executor(DBMS &dbms) : dbms_(dbms) {
}

// endregion

// region Core

std::string Executor::execute(const Statement &stmt) {
    return std::visit(
        Overloaded{
            [this](const CreateDatabaseStmt &s) { return exec_create_database(s); },
            [this](const DropDatabaseStmt &s) { return exec_drop_database(s); },
            [this](const UseStmt &s) { return exec_use(s); },
            [this](const CreateTableStmt &s) { return exec_create_table(s); },
            [this](const DropTableStmt &s) { return exec_drop_table(s); },
            [this](const InsertStmt &s) { return exec_insert(s); },
            [this](const UpdateStmt &s) { return exec_update(s); },
            [this](const DeleteStmt &s) { return exec_delete(s); },
            [this](const SelectStmt &s) { return exec_select(s); }
        },
        stmt);
}

// endregion

// region Utilities

Database &Executor::resolve_db(const std::string &db_name) {
    if (!db_name.empty()) {
        return dbms_.get_database(db_name);
    }

    Database *current = dbms_.current_database();
    if (current == nullptr) {
        throw std::runtime_error("No database selected");
    }
    return *current;
}

Table &Executor::resolve_table(const std::string &db_name, const std::string &table_name) {
    return resolve_db(db_name).get_table(table_name);
}

// endregion

// region Database Operations

std::string Executor::exec_create_database(const CreateDatabaseStmt &s) const {
    dbms_.create_database(s.db_name);
    return ok_json("Database '" + s.db_name + "' created").dump();
}

std::string Executor::exec_drop_database(const DropDatabaseStmt &s) const {
    dbms_.drop_database(s.db_name);
    return ok_json("Database '" + s.db_name + "' dropped").dump();
}

std::string Executor::exec_use(const UseStmt &s) const {
    dbms_.use(s.db_name);
    return ok_json("Using database '" + s.db_name + "'").dump();
}

// endregion

// region Table Operations

std::string Executor::exec_create_table(const CreateTableStmt &s) {
    Database &db = resolve_db(s.db_name);
    std::set<std::string> names;
    for (const Column &column : s.schema) {
        ensure_no_duplicate(names, column.name);
        names.insert(column.name);
    }

    db.create_table(s.table_name, s.schema);
    return ok_json("Table '" + s.table_name + "' created").dump();
}

std::string Executor::exec_drop_table(const DropTableStmt &s) {
    resolve_db(s.db_name).drop_table(s.table_name);
    return ok_json("Table '" + s.table_name + "' dropped").dump();
}

// endregion

// region Row Operations

std::string Executor::exec_insert(const InsertStmt &s) {
    Table &table = resolve_table(s.db_name, s.table_name);
    const Schema &schema = table.schema();

    std::vector<int> column_indexes;
    column_indexes.reserve(s.columns.size());

    std::set<std::string> seen;
    for (const std::string &column : s.columns) {
        ensure_no_duplicate(seen, column);
        seen.insert(column);
        column_indexes.push_back(require_column(schema, column));
    }

    std::size_t inserted = 0;
    for (const Row &input_row : s.rows) {
        if (input_row.size() != column_indexes.size()) {
            throw std::runtime_error("INSERT row value count does not match column count");
        }

        Row row(schema.size(), nullptr);
        for (std::size_t i = 0; i < input_row.size(); ++i) {
            row[static_cast<std::size_t>(column_indexes[i])] = input_row[i];
        }

        table.insert(row);
        ++inserted;
    }

    return count_json("insert", inserted).dump();
}

std::string Executor::exec_update(const UpdateStmt &s) {
    Table &table = resolve_table(s.db_name, s.table_name);
    const Schema &schema = table.schema();

    std::vector<std::pair<int, Expr>> assignments;
    assignments.reserve(s.assignments.size());

    std::set<std::string> seen;
    for (const auto &[column, expr] : s.assignments) {
        ensure_no_duplicate(seen, column);
        seen.insert(column);
        assignments.push_back({require_column(schema, column), expr});
    }

    std::size_t updated = 0;
    const std::vector<Row> &data = table.data();
    for (RowID id = 0; id < data.size(); ++id) {
        if (table.is_deleted(id)) {
            continue;
        }
        if (s.where && !eval_condition(*s.where, data[id], schema)) {
            continue;
        }

        Row new_row = data[id];
        for (const auto &[index, expr] : assignments) {
            new_row[static_cast<std::size_t>(index)] = eval_expr(expr, data[id], schema);
        }
        table.update(id, std::move(new_row));
        ++updated;
    }

    return count_json("update", updated).dump();
}

std::string Executor::exec_delete(const DeleteStmt &s) {
    Table &table = resolve_table(s.db_name, s.table_name);
    const Schema &schema = table.schema();

    std::size_t deleted = 0;
    const std::vector<Row> &data = table.data();
    for (RowID id = 0; id < data.size(); ++id) {
        if (table.is_deleted(id)) {
            continue;
        }
        if (s.where && !eval_condition(*s.where, data[id], schema)) {
            continue;
        }

        table.erase(id);
        ++deleted;
    }

    return count_json("delete", deleted).dump();
}

std::string Executor::exec_select(const SelectStmt &s) {
    Table &table = resolve_table(s.db_name, s.table_name);
    const Schema &schema = table.schema();

    std::vector<int> selected_indexes;
    std::vector<std::string> output_names;

    if (s.star) {
        selected_indexes.reserve(schema.size());
        output_names.reserve(schema.size());
        for (std::size_t i = 0; i < schema.size(); ++i) {
            selected_indexes.push_back(static_cast<int>(i));
            output_names.push_back(schema[i].name);
        }
    } else {
        selected_indexes.reserve(s.columns.size());
        output_names.reserve(s.columns.size());
        for (const SelectColumn &column : s.columns) {
            selected_indexes.push_back(require_column(schema, column.name));
            output_names.push_back(column.alias.empty() ? column.name : column.alias);
        }
    }

    std::vector<Row> rows;
    const std::vector<Row> &data = table.data();
    for (RowID id = 0; id < data.size(); ++id) {
        if (table.is_deleted(id)) {
            continue;
        }
        if (s.where && !eval_condition(*s.where, data[id], schema)) {
            continue;
        }

        Row projected;
        projected.reserve(selected_indexes.size());
        for (const int index : selected_indexes) {
            projected.push_back(data[id][static_cast<std::size_t>(index)]);
        }
        rows.push_back(std::move(projected));
    }

    return rows_to_json(rows, output_names);
}

// endregion

// region Evaluation

bool Executor::eval_condition(const Condition &cond, const Row &row, const Schema &schema) {
    switch (cond.kind) {
        case ConditionKind::Simple: {
            if (!cond.predicate) {
                throw std::runtime_error("Malformed simple condition");
            }

            const Value lhs = eval_expr(cond.predicate->lhs, row, schema);
            const Value rhs = eval_expr(cond.predicate->rhs, row, schema);

            if (cond.predicate->op == CmpOp::EQ) {
                return lhs == rhs;
            }
            if (cond.predicate->op == CmpOp::NEQ) {
                return lhs != rhs;
            }
            return compare_values(lhs, rhs) < 0 && cond.predicate->op == CmpOp::LT
                || compare_values(lhs, rhs) > 0 && cond.predicate->op == CmpOp::GT
                || compare_values(lhs, rhs) <= 0 && cond.predicate->op == CmpOp::LEQ
                || compare_values(lhs, rhs) >= 0 && cond.predicate->op == CmpOp::GEQ;
        }

        case ConditionKind::Between: {
            if (!cond.between) {
                throw std::runtime_error("Malformed BETWEEN condition");
            }

            const Value lhs = eval_expr(cond.between->lhs, row, schema);
            const Value lo = eval_expr(cond.between->lo, row, schema);
            const Value hi = eval_expr(cond.between->hi, row, schema);
            return compare_values(lhs, lo) >= 0 && compare_values(lhs, hi) < 0;
        }

        case ConditionKind::Like: {
            if (!cond.like) {
                throw std::runtime_error("Malformed LIKE condition");
            }

            const Value lhs = eval_expr(cond.like->lhs, row, schema);
            if (!std::holds_alternative<std::string>(lhs)) {
                throw std::runtime_error("LIKE expects a string left operand");
            }

            return std::regex_match(std::get<std::string>(lhs), std::regex(cond.like->pattern));
        }

        case ConditionKind::And:
            if (!cond.left || !cond.right) {
                throw std::runtime_error("Malformed AND condition");
            }
            return eval_condition(*cond.left, row, schema) && eval_condition(*cond.right, row, schema);

        case ConditionKind::Or:
            if (!cond.left || !cond.right) {
                throw std::runtime_error("Malformed OR condition");
            }
            return eval_condition(*cond.left, row, schema) || eval_condition(*cond.right, row, schema);
    }

    throw std::runtime_error("Unknown condition kind");
}

Value Executor::eval_expr(const Expr &expr, const Row &row, const Schema &schema) {
    if (expr.kind == ExprKind::Literal) {
        return expr.literal;
    }

    const int index = require_column(schema, expr.column);
    return row[static_cast<std::size_t>(index)];
}

// endregion

// region Serialization

std::string Executor::rows_to_json(const std::vector<Row> &rows, const std::vector<std::string> &col_names) {
    nlohmann::json result = nlohmann::json::array();

    for (const Row &row : rows) {
        if (row.size() != col_names.size()) {
            throw std::runtime_error("Internal error: selected row width does not match column names");
        }

        nlohmann::json object = nlohmann::json::object();
        for (std::size_t i = 0; i < row.size(); ++i) {
            object[col_names[i]] = value_to_json(row[i]);
        }
        result.push_back(std::move(object));
    }

    return result.dump();
}

// endregion
