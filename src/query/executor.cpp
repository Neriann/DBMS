#include "query/executor.hpp"

#include <nlohmann/json.hpp>
#include <limits>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
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

std::unordered_map<std::string, int> build_column_index_map(const Schema &schema) {
    std::unordered_map<std::string, int> indexes;
    indexes.reserve(schema.size());
    for (std::size_t i = 0; i < schema.size(); ++i) {
        indexes.emplace(schema[i].name, static_cast<int>(i));
    }
    return indexes;
}

int require_column(
    const std::unordered_map<std::string, int> &column_indexes,
    const std::string &name) {
    const auto it = column_indexes.find(name);
    if (it == column_indexes.end()) {
        throw std::runtime_error("Unknown column '" + name + "'");
    }
    return it->second;
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

void validate_default_value(const Column &column, const Value &value) {
    if (std::holds_alternative<std::nullptr_t>(value)) {
        if (column.is_not_null() || column.is_indexed()) {
            throw std::runtime_error("DEFAULT NULL is not allowed for column '" + column.name + "'");
        }
        return;
    }

    if (column.type == ColumnType::INT && !std::holds_alternative<int>(value)) {
        throw std::runtime_error("DEFAULT value for column '" + column.name + "' must be INT");
    }
    if (column.type == ColumnType::STRING && !std::holds_alternative<std::string>(value)) {
        throw std::runtime_error("DEFAULT value for column '" + column.name + "' must be STRING");
    }
}

std::string aggregate_function_name(const AggregateFunction function) {
    switch (function) {
        case AggregateFunction::Sum:
            return "SUM";
        case AggregateFunction::Count:
            return "COUNT";
        case AggregateFunction::Avg:
            return "AVG";
    }

    throw std::runtime_error("Unknown aggregate function");
}

std::string select_item_output_name(const SelectItem &item) {
    return std::visit(
        Overloaded{
            [](const SelectColumn &column) {
                return column.alias.empty() ? column.name : column.alias;
            },
            [](const AggregateCall &aggregate) {
                if (!aggregate.alias.empty()) {
                    return aggregate.alias;
                }
                if (aggregate.count_star) {
                    return aggregate_function_name(aggregate.function) + "(*)";
                }
                return aggregate_function_name(aggregate.function) + "(" + aggregate.column + ")";
            }
        },
        item);
}

void ensure_unique_output_names(const std::vector<std::string> &output_names) {
    std::set<std::string> seen_output_names;
    for (const std::string &output_name : output_names) {
        if (!seen_output_names.insert(output_name).second) {
            throw std::invalid_argument("Duplicate projected output name: " + output_name);
        }
    }
}

nlohmann::json eval_aggregate(
    const AggregateCall &aggregate,
    const Schema &schema,
    const std::unordered_map<std::string, int> &column_indexes,
    const std::vector<Row> &data,
    const std::vector<RowID> &row_ids) {
    if (aggregate.function == AggregateFunction::Count && aggregate.count_star) {
        return row_ids.size();
    }

    const int column_index = require_column(column_indexes, aggregate.column);
    const std::size_t index = static_cast<std::size_t>(column_index);

    if (aggregate.function == AggregateFunction::Count) {
        std::size_t count = 0;
        for (const RowID id : row_ids) {
            if (!std::holds_alternative<std::nullptr_t>(data[id][index])) {
                ++count;
            }
        }
        return count;
    }

    if (schema[index].type != ColumnType::INT) {
        throw std::runtime_error(aggregate_function_name(aggregate.function)
            + " expects an INT column: '" + aggregate.column + "'");
    }

    long long sum = 0;
    std::size_t count = 0;
    for (const RowID id : row_ids) {
        const Value &value = data[id][index];
        if (std::holds_alternative<std::nullptr_t>(value)) {
            continue;
        }
        if (!std::holds_alternative<int>(value)) {
            throw std::runtime_error(aggregate_function_name(aggregate.function)
                + " expects an INT column: '" + aggregate.column + "'");
        }
        sum += std::get<int>(value);
        ++count;
    }

    if (count == 0) {
        return nullptr;
    }
    if (aggregate.function == AggregateFunction::Sum) {
        return sum;
    }
    if (aggregate.function == AggregateFunction::Avg) {
        return static_cast<double>(sum) / static_cast<double>(count);
    }

    throw std::runtime_error("Unknown aggregate function");
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

    for (const Column &column : s.schema) {
        if (column.default_value) {
            validate_default_value(column, *column.default_value);
        }
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
    const std::unordered_map<std::string, int> schema_indexes = build_column_index_map(schema);

    std::vector<int> column_indexes;
    column_indexes.reserve(s.columns.size());

    std::set<std::string> seen;
    for (const std::string &column : s.columns) {
        ensure_no_duplicate(seen, column);
        seen.insert(column);
        column_indexes.push_back(require_column(schema_indexes, column));
    }

    std::vector<Row> rows;
    rows.reserve(s.rows.size());
    for (const Row &input_row : s.rows) {
        if (input_row.size() != column_indexes.size()) {
            throw std::runtime_error("INSERT row value count does not match column count");
        }

        Row row(schema.size(), nullptr);
        for (std::size_t i = 0; i < schema.size(); ++i) {
            if (schema[i].default_value) {
                row[i] = *schema[i].default_value;
            }
        }

        for (std::size_t i = 0; i < input_row.size(); ++i) {
            row[static_cast<std::size_t>(column_indexes[i])] = input_row[i];
        }

        rows.push_back(std::move(row));
    }

    const std::size_t inserted = rows.size();
    table.insert_many(rows);

    return count_json("insert", inserted).dump();
}

std::string Executor::exec_update(const UpdateStmt &s) {
    Table &table = resolve_table(s.db_name, s.table_name);
    const Schema &schema = table.schema();
    const std::unordered_map<std::string, int> column_indexes = build_column_index_map(schema);

    std::vector<std::pair<int, Expr>> assignments;
    assignments.reserve(s.assignments.size());

    std::set<std::string> seen;
    for (const auto &[column, expr] : s.assignments) {
        ensure_no_duplicate(seen, column);
        seen.insert(column);
        assignments.push_back({require_column(column_indexes, column), expr});
    }

    std::vector<std::pair<RowID, Row>> updates;
    const std::vector<Row> &data = table.data();
    for (RowID id = 0; id < data.size(); ++id) {
        if (table.is_deleted(id)) {
            continue;
        }
        if (s.where && !eval_condition(*s.where, data[id], column_indexes)) {
            continue;
        }

        Row new_row = data[id];
        for (const auto &[index, expr] : assignments) {
            new_row[static_cast<std::size_t>(index)] = eval_expr(expr, data[id], column_indexes);
        }
        updates.emplace_back(id, std::move(new_row));
    }

    const std::size_t updated = updates.size();
    table.update_many(std::move(updates));

    return count_json("update", updated).dump();
}

std::string Executor::exec_delete(const DeleteStmt &s) {
    Table &table = resolve_table(s.db_name, s.table_name);
    const Schema &schema = table.schema();
    const std::unordered_map<std::string, int> column_indexes = build_column_index_map(schema);

    std::vector<RowID> matching_ids;
    const std::vector<Row> &data = table.data();
    for (RowID id = 0; id < data.size(); ++id) {
        if (table.is_deleted(id)) {
            continue;
        }
        if (s.where && !eval_condition(*s.where, data[id], column_indexes)) {
            continue;
        }

        matching_ids.push_back(id);
    }

    for (const RowID id : matching_ids) {
        table.erase(id);
    }

    return count_json("delete", matching_ids.size()).dump();
}

std::string Executor::exec_select(const SelectStmt &s) {
    Table &table = resolve_table(s.db_name, s.table_name);
    const Schema &schema = table.schema();
    const std::unordered_map<std::string, int> column_indexes = build_column_index_map(schema);

    std::vector<RowID> matching_ids;
    const std::vector<Row> &data = table.data();
    for (RowID id = 0; id < data.size(); ++id) {
        if (table.is_deleted(id)) {
            continue;
        }
        if (s.where && !eval_condition(*s.where, data[id], column_indexes)) {
            continue;
        }
        matching_ids.push_back(id);
    }

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
        bool has_aggregate = false;
        bool has_column = false;
        for (const SelectItem &item : s.items) {
            has_aggregate = has_aggregate || std::holds_alternative<AggregateCall>(item);
            has_column = has_column || std::holds_alternative<SelectColumn>(item);
        }

        if (has_aggregate && has_column) {
            throw std::invalid_argument("Cannot mix aggregate and non-aggregate select items without GROUP BY");
        }

        if (has_aggregate) {
            output_names.reserve(s.items.size());
            for (const SelectItem &item : s.items) {
                output_names.push_back(select_item_output_name(item));
            }
            ensure_unique_output_names(output_names);

            nlohmann::json object = nlohmann::json::object();
            for (std::size_t i = 0; i < s.items.size(); ++i) {
                object[output_names[i]] = eval_aggregate(
                    std::get<AggregateCall>(s.items[i]),
                    schema,
                    column_indexes,
                    data,
                    matching_ids);
            }

            nlohmann::json result = nlohmann::json::array();
            result.push_back(std::move(object));
            return result.dump();
        }

        selected_indexes.reserve(s.items.size());
        output_names.reserve(s.items.size());
        for (const SelectItem &item : s.items) {
            const SelectColumn &column = std::get<SelectColumn>(item);
            selected_indexes.push_back(require_column(column_indexes, column.name));
            output_names.push_back(select_item_output_name(item));
        }
    }

    ensure_unique_output_names(output_names);

    std::vector<Row> rows;
    rows.reserve(matching_ids.size());
    for (const RowID id : matching_ids) {
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

bool Executor::eval_condition(
    const Condition &cond,
    const Row &row,
    const std::unordered_map<std::string, int> &column_indexes) {
    switch (cond.kind) {
        case ConditionKind::Simple: {
            if (!cond.predicate) {
                throw std::runtime_error("Malformed simple condition");
            }

            const Value lhs = eval_expr(cond.predicate->lhs, row, column_indexes);
            const Value rhs = eval_expr(cond.predicate->rhs, row, column_indexes);

            if (cond.predicate->op == CmpOp::EQ) {
                return lhs == rhs;
            }
            if (cond.predicate->op == CmpOp::NEQ) {
                return lhs != rhs;
            }

            const int ordering = compare_values(lhs, rhs);
            switch (cond.predicate->op) {
                case CmpOp::LT:
                    return ordering < 0;
                case CmpOp::GT:
                    return ordering > 0;
                case CmpOp::LEQ:
                    return ordering <= 0;
                case CmpOp::GEQ:
                    return ordering >= 0;
                default:
                    throw std::runtime_error("Unknown comparison operator");
            }
        }

        case ConditionKind::Between: {
            if (!cond.between) {
                throw std::runtime_error("Malformed BETWEEN condition");
            }

            const Value lhs = eval_expr(cond.between->lhs, row, column_indexes);
            const Value lo = eval_expr(cond.between->lo, row, column_indexes);
            const Value hi = eval_expr(cond.between->hi, row, column_indexes);
            return compare_values(lhs, lo) >= 0 && compare_values(lhs, hi) < 0;
        }

        case ConditionKind::Like: {
            if (!cond.like) {
                throw std::runtime_error("Malformed LIKE condition");
            }

            const Value lhs = eval_expr(cond.like->lhs, row, column_indexes);
            const Value rhs = eval_expr(cond.like->rhs, row, column_indexes);
            if (!std::holds_alternative<std::string>(lhs)) {
                throw std::runtime_error("LIKE expects a string left operand");
            }
            if (!std::holds_alternative<std::string>(rhs)) {
                throw std::runtime_error("LIKE expects a string right operand");
            }

            try {
                const std::regex pattern(std::get<std::string>(rhs));
                return std::regex_match(std::get<std::string>(lhs), pattern);
            } catch (const std::regex_error &) {
                throw std::runtime_error("Invalid LIKE regex pattern");
            }
        }

        case ConditionKind::And:
            if (!cond.left || !cond.right) {
                throw std::runtime_error("Malformed AND condition");
            }
            return eval_condition(*cond.left, row, column_indexes)
                && eval_condition(*cond.right, row, column_indexes);

        case ConditionKind::Or:
            if (!cond.left || !cond.right) {
                throw std::runtime_error("Malformed OR condition");
            }
            return eval_condition(*cond.left, row, column_indexes)
                || eval_condition(*cond.right, row, column_indexes);
    }

    throw std::runtime_error("Unknown condition kind");
}

Value Executor::eval_expr(
    const Expr &expr,
    const Row &row,
    const std::unordered_map<std::string, int> &column_indexes) {
    if (expr.kind == ExprKind::Literal) {
        return expr.literal;
    }

    if (expr.kind == ExprKind::Column) {
        const int index = require_column(column_indexes, expr.column);
        return row[static_cast<std::size_t>(index)];
    }

    if (expr.kind == ExprKind::UnaryMinus) {
        if (!expr.operand) {
            throw std::runtime_error("Malformed unary minus expression");
        }
        const Value operand_value = eval_expr(*expr.operand, row, column_indexes);
        if (!std::holds_alternative<int>(operand_value)) {
            throw std::runtime_error("Unary minus expects an integer operand");
        }
        const int value = std::get<int>(operand_value);
        if (value == std::numeric_limits<int>::min()) {
            throw std::runtime_error("Unary minus result is out of range");
        }
        return -value;
    }

    throw std::runtime_error("Unknown expression kind");
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
