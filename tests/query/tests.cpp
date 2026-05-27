#include "core/dbms.hpp"
#include "query/scanner.hpp"
#include "parser.hpp"
#include "query/executor.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace {

std::vector<Statement> parse_sql(const std::string &sql) {
    std::istringstream input(sql);
    Scanner scanner(input);
    std::vector<Statement> statements;
    yy::Parser parser(scanner, statements);
    parser.parse();
    return statements;
}

std::vector<std::string> execute_sql(DBMS &dbms, const std::string &sql) {
    Executor executor(dbms);
    std::vector<std::string> results;

    for (const Statement &statement : parse_sql(sql)) {
        results.push_back(executor.execute(statement));
    }

    return results;
}

nlohmann::json json_of(const std::string &text) {
    return nlohmann::json::parse(text);
}

void expect_json_eq(const std::string &actual, const std::string &expected) {
    EXPECT_EQ(json_of(actual), json_of(expected))
        << "actual:   " << json_of(actual).dump()
        << "\nexpected: " << json_of(expected).dump();
}

template <typename T>
const T &as_statement(const Statement &statement) {
    return std::get<T>(statement);
}

} // namespace

TEST(QueryParser, ParsesAllBaseStatementKinds) {
    const std::vector<Statement> statements = parse_sql(
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE users (id INT INDEXED, name STRING NOT NULL);"
        "INSERT INTO users (id, name) VALUE (1, \"Ann\");"
        "UPDATE users SET name = \"Alice\" WHERE id == 1;"
        "SELECT (id AS user_id, name) FROM users WHERE id BETWEEN 1 AND 2;"
        "DELETE FROM users WHERE name LIKE \"A.*\";"
        "DROP TABLE users;"
        "DROP DATABASE app;");

    ASSERT_EQ(statements.size(), 9);
    EXPECT_TRUE(std::holds_alternative<CreateDatabaseStmt>(statements[0]));
    EXPECT_TRUE(std::holds_alternative<UseStmt>(statements[1]));
    EXPECT_TRUE(std::holds_alternative<CreateTableStmt>(statements[2]));
    EXPECT_TRUE(std::holds_alternative<InsertStmt>(statements[3]));
    EXPECT_TRUE(std::holds_alternative<UpdateStmt>(statements[4]));
    EXPECT_TRUE(std::holds_alternative<SelectStmt>(statements[5]));
    EXPECT_TRUE(std::holds_alternative<DeleteStmt>(statements[6]));
    EXPECT_TRUE(std::holds_alternative<DropTableStmt>(statements[7]));
    EXPECT_TRUE(std::holds_alternative<DropDatabaseStmt>(statements[8]));
}

TEST(QueryParser, ParsesParenthesizedBooleanConditionsWithPrecedence) {
    const std::vector<Statement> statements = parse_sql(
        "SELECT * FROM users WHERE (id == 1 OR id == 2) AND name != \"Bob\";");

    ASSERT_EQ(statements.size(), 1);
    const auto &select = as_statement<SelectStmt>(statements[0]);
    ASSERT_TRUE(select.where.has_value());
    EXPECT_EQ(select.where->kind, ConditionKind::And);
    ASSERT_TRUE(select.where->left);
    ASSERT_TRUE(select.where->right);
    EXPECT_EQ(select.where->left->kind, ConditionKind::Or);
    EXPECT_EQ(select.where->right->kind, ConditionKind::Simple);
}

TEST(QueryParser, ParsesDefaultColumnAttributes) {
    const std::vector<Statement> statements = parse_sql(
        "CREATE TABLE users ("
        "id INT INDEXED DEFAULT 1,"
        "name STRING NOT NULL DEFAULT \"unknown\","
        "age INT DEFAULT NULL"
        ");");

    ASSERT_EQ(statements.size(), 1);
    const auto &create = as_statement<CreateTableStmt>(statements[0]);
    ASSERT_EQ(create.schema.size(), 3);

    ASSERT_TRUE(create.schema[0].default_value.has_value());
    EXPECT_EQ(std::get<int>(*create.schema[0].default_value), 1);

    ASSERT_TRUE(create.schema[1].default_value.has_value());
    EXPECT_EQ(std::get<std::string>(*create.schema[1].default_value), "unknown");

    ASSERT_TRUE(create.schema[2].default_value.has_value());
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(*create.schema[2].default_value));
}

TEST(QueryParser, ParsesSelectItemsAsColumnsOrAggregateCalls) {
    const std::vector<Statement> statements = parse_sql(
        "SELECT id AS user_id, COUNT(*), SUM(age) AS total_age, AVG(age) FROM users;");

    ASSERT_EQ(statements.size(), 1);
    const auto &select = as_statement<SelectStmt>(statements[0]);
    ASSERT_FALSE(select.star);
    ASSERT_EQ(select.items.size(), 4);

    ASSERT_TRUE(std::holds_alternative<SelectColumn>(select.items[0]));
    const auto &column = std::get<SelectColumn>(select.items[0]);
    EXPECT_EQ(column.name, "id");
    EXPECT_EQ(column.alias, "user_id");

    ASSERT_TRUE(std::holds_alternative<AggregateCall>(select.items[1]));
    const auto &count = std::get<AggregateCall>(select.items[1]);
    EXPECT_EQ(count.function, AggregateFunction::Count);
    EXPECT_TRUE(count.count_star);
    EXPECT_TRUE(count.column.empty());

    ASSERT_TRUE(std::holds_alternative<AggregateCall>(select.items[2]));
    const auto &sum = std::get<AggregateCall>(select.items[2]);
    EXPECT_EQ(sum.function, AggregateFunction::Sum);
    EXPECT_EQ(sum.column, "age");
    EXPECT_FALSE(sum.count_star);
    EXPECT_EQ(sum.alias, "total_age");

    ASSERT_TRUE(std::holds_alternative<AggregateCall>(select.items[3]));
    const auto &avg = std::get<AggregateCall>(select.items[3]);
    EXPECT_EQ(avg.function, AggregateFunction::Avg);
    EXPECT_EQ(avg.column, "age");
    EXPECT_FALSE(avg.count_star);
}

TEST(QueryParser, RejectsSyntaxErrorsInvalidCharactersAndMixedCaseKeywords) {
    EXPECT_THROW(parse_sql("CREATE DATABASE app"), std::exception);
    EXPECT_THROW(parse_sql("CREATE DATABASE @bad;"), std::exception);
    EXPECT_THROW(parse_sql("Create DATABASE app;"), std::exception);
    EXPECT_THROW(parse_sql("CREATE TABLE t (id NUMBER);"), std::exception);
    EXPECT_THROW(parse_sql("CREATE DATABASE 1bad;"), std::exception);
    EXPECT_THROW(parse_sql("CREATE TABLE empty ();"), std::exception);
    EXPECT_THROW(parse_sql("INSERT INTO users () VALUE ();"), std::exception);
}

TEST(QueryParser, RejectsIntegerOverflow) {
    EXPECT_THROW(parse_sql("INSERT INTO t (id) VALUE (999999999999999999999999);"), std::exception);
}

TEST(QueryParser, AcceptsEmptyInputCommentsWhitespaceAndLowercaseKeywords) {
    EXPECT_TRUE(parse_sql("").empty());
    EXPECT_TRUE(parse_sql("   \n\t -- only comment\n").empty());

    const std::vector<Statement> statements = parse_sql(
        "-- leading comment\n"
        "create database app;\n"
        "use app;\n"
        "create table users (\n"
        "  id int indexed,\n"
        "  name string not null\n"
        ");\n");

    ASSERT_EQ(statements.size(), 3);
    EXPECT_TRUE(std::holds_alternative<CreateDatabaseStmt>(statements[0]));
    EXPECT_TRUE(std::holds_alternative<UseStmt>(statements[1]));
    EXPECT_TRUE(std::holds_alternative<CreateTableStmt>(statements[2]));
}

TEST(QueryParser, RejectsBadStringEscapesAndUnterminatedStrings) {
    EXPECT_THROW(parse_sql("INSERT INTO t (s) VALUE (\"bad\\q\");"), std::exception);
    EXPECT_THROW(parse_sql("INSERT INTO t (s) VALUE (\"unterminated);"), std::exception);
}

TEST(QueryParser, RejectsDuplicateDefaultModifier) {
    EXPECT_THROW(
        parse_sql("CREATE TABLE users (name STRING DEFAULT \"a\" DEFAULT \"b\");"),
        std::exception);
}

TEST(QueryExecutor, RunsCrudSelectAliasesBetweenLikeAndDelete) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE users (id INT INDEXED, name STRING NOT NULL, age INT);"
        "INSERT INTO users (id, name, age) VALUE (1, \"Ann\", 20), (2, \"Bob\", 25), (3, \"Alice\", 30);"
        "SELECT (id AS user_id, name) FROM users WHERE id BETWEEN 1 AND 3;"
        "UPDATE users SET age = 31 WHERE name == \"Alice\";"
        "SELECT * FROM users WHERE name LIKE \"A.*\" AND age >= 20;"
        "DELETE FROM users WHERE id == 2 OR age >= 31;"
        "SELECT * FROM users;");

    ASSERT_EQ(results.size(), 9);
    expect_json_eq(results[0], R"({"message":"Database 'app' created","status":"ok"})");
    expect_json_eq(results[3], R"({"count":3,"operation":"insert","status":"ok"})");
    expect_json_eq(results[4], R"([{"name":"Ann","user_id":1},{"name":"Bob","user_id":2}])");
    expect_json_eq(results[5], R"({"count":1,"operation":"update","status":"ok"})");
    expect_json_eq(results[6], R"([{"age":20,"id":1,"name":"Ann"},{"age":31,"id":3,"name":"Alice"}])");
    expect_json_eq(results[7], R"({"count":2,"operation":"delete","status":"ok"})");
    expect_json_eq(results[8], R"([{"age":20,"id":1,"name":"Ann"}])");
}

TEST(QueryExecutor, SupportsSelectWithoutParenthesesAndStarProjection) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE users (id INT, name STRING);"
        "INSERT INTO users (id, name) VALUE (1, \"Ann\");"
        "SELECT id AS user_id, name FROM users;"
        "SELECT * FROM users;");

    ASSERT_EQ(results.size(), 6);
    expect_json_eq(results[4], R"([{"name":"Ann","user_id":1}])");
    expect_json_eq(results[5], R"([{"id":1,"name":"Ann"}])");
}

TEST(QueryExecutor, RejectsDuplicateProjectedOutputNames) {
    DBMS dbms;
    execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE users (id INT, name STRING);"
        "INSERT INTO users (id, name) VALUE (1, \"Ann\");");

    EXPECT_THROW(execute_sql(dbms, "SELECT id AS x, name AS x FROM users;"), std::invalid_argument);
    EXPECT_THROW(execute_sql(dbms, "SELECT id, id FROM users;"), std::invalid_argument);
}

TEST(QueryExecutor, UsesCurrentDatabaseOrExplicitDatabaseName) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "CREATE TABLE app.users (id INT, name STRING);"
        "INSERT INTO app.users (id, name) VALUE (1, \"Ann\");"
        "USE app;"
        "SELECT * FROM users;");

    ASSERT_EQ(results.size(), 5);
    expect_json_eq(results.back(), R"([{"id":1,"name":"Ann"}])");
}

TEST(QueryExecutor, HandlesNullsAndOmittedNullableColumns) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE users (id INT INDEXED, name STRING, age INT);"
        "INSERT INTO users (id, name) VALUE (1, NULL);"
        "SELECT * FROM users;");

    ASSERT_EQ(results.size(), 5);
    expect_json_eq(results.back(), R"([{"age":null,"id":1,"name":null}])");
}

TEST(QueryExecutor, AppliesDefaultValuesOnlyForOmittedColumns) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE users ("
        "id INT INDEXED,"
        "name STRING DEFAULT \"anonymous\","
        "age INT DEFAULT 18,"
        "note STRING DEFAULT NULL"
        ");"
        "INSERT INTO users (id) VALUE (1);"
        "INSERT INTO users (id, name, age, note) VALUE (2, NULL, NULL, \"explicit\");"
        "SELECT * FROM users;");

    ASSERT_EQ(results.size(), 6);
    expect_json_eq(
        results.back(),
        R"([{"age":18,"id":1,"name":"anonymous","note":null},{"age":null,"id":2,"name":null,"note":"explicit"}])");
}

TEST(QueryExecutor, DefaultValuesSatisfyNotNullAndIndexedColumns) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE singleton (id INT INDEXED DEFAULT 1, name STRING NOT NULL DEFAULT \"root\");"
        "INSERT INTO singleton (name) VALUE (\"first\");"
        "SELECT * FROM singleton;");

    ASSERT_EQ(results.size(), 5);
    expect_json_eq(results.back(), R"([{"id":1,"name":"first"}])");

    EXPECT_THROW(execute_sql(dbms, "INSERT INTO singleton (name) VALUE (\"duplicate default id\");"), std::exception);
}

TEST(QueryExecutor, RejectsInvalidDefaultValuesWhenCreatingTable) {
    DBMS dbms;
    execute_sql(dbms, "CREATE DATABASE app; USE app;");

    EXPECT_THROW(execute_sql(dbms, "CREATE TABLE bad_int (value INT DEFAULT \"bad\");"), std::runtime_error);
    EXPECT_THROW(execute_sql(dbms, "CREATE TABLE bad_string (value STRING DEFAULT 10);"), std::runtime_error);
    EXPECT_THROW(execute_sql(dbms, "CREATE TABLE bad_not_null (value STRING NOT NULL DEFAULT NULL);"), std::runtime_error);
    EXPECT_THROW(execute_sql(dbms, "CREATE TABLE bad_indexed (value INT INDEXED DEFAULT NULL);"), std::runtime_error);
}

TEST(QueryExecutor, BetweenIsHalfOpenInterval) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE numbers (value INT);"
        "INSERT INTO numbers (value) VALUE (1), (2), (3);"
        "SELECT * FROM numbers WHERE value BETWEEN 1 AND 3;");

    ASSERT_EQ(results.size(), 5);
    expect_json_eq(results.back(), R"([{"value":1},{"value":2}])");
}

TEST(QueryExecutor, EvaluatesBooleanPrecedenceAndParentheses) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE users (id INT, name STRING, active INT);"
        "INSERT INTO users (id, name, active) VALUE (1, \"Ann\", 1), (2, \"Bob\", 0), (3, \"Cat\", 0);"
        "SELECT id FROM users WHERE id == 1 OR id == 2 AND active == 1;"
        "SELECT id FROM users WHERE (id == 1 OR id == 2) AND active == 0;");

    ASSERT_EQ(results.size(), 6);
    expect_json_eq(results[4], R"([{"id":1}])");
    expect_json_eq(results[5], R"([{"id":2}])");
}

TEST(QueryExecutor, ComparesStringsLexicographically) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE words (word STRING);"
        "INSERT INTO words (word) VALUE (\"ant\"), (\"bee\"), (\"cat\");"
        "SELECT * FROM words WHERE word >= \"bee\";");

    ASSERT_EQ(results.size(), 5);
    expect_json_eq(results.back(), R"([{"word":"bee"},{"word":"cat"}])");
}

TEST(QueryExecutor, CanAssignColumnExpressionInUpdate) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE users (id INT, name STRING, backup STRING);"
        "INSERT INTO users (id, name, backup) VALUE (1, \"Ann\", NULL);"
        "UPDATE users SET backup = name WHERE id == 1;"
        "SELECT * FROM users;");

    ASSERT_EQ(results.size(), 6);
    expect_json_eq(results[4], R"({"count":1,"operation":"update","status":"ok"})");
    expect_json_eq(results[5], R"([{"backup":"Ann","id":1,"name":"Ann"}])");
}

TEST(QueryExecutor, DeleteWithoutWhereDeletesAllRows) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE users (id INT);"
        "INSERT INTO users (id) VALUE (1), (2);"
        "DELETE FROM users;"
        "SELECT * FROM users;");

    ASSERT_EQ(results.size(), 6);
    expect_json_eq(results[4], R"({"count":2,"operation":"delete","status":"ok"})");
    expect_json_eq(results[5], R"([])");
}

TEST(QueryExecutor, DropsTablesAndDatabases) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE users (id INT);"
        "DROP TABLE users;"
        "DROP DATABASE app;");

    ASSERT_EQ(results.size(), 5);
    expect_json_eq(results[3], R"({"message":"Table 'users' dropped","status":"ok"})");
    expect_json_eq(results[4], R"({"message":"Database 'app' dropped","status":"ok"})");
    EXPECT_FALSE(dbms.has_database("app"));
}

TEST(QueryExecutor, RejectsMissingDatabaseContext) {
    DBMS dbms;
    EXPECT_THROW(execute_sql(dbms, "CREATE TABLE users (id INT);"), std::exception);
}

TEST(QueryExecutor, RejectsDuplicateDatabaseAndTableNames) {
    DBMS dbms;
    execute_sql(dbms, "CREATE DATABASE app; USE app; CREATE TABLE users (id INT);");
    EXPECT_THROW(execute_sql(dbms, "CREATE DATABASE app;"), std::exception);
    EXPECT_THROW(execute_sql(dbms, "CREATE TABLE users (id INT);"), std::exception);
}

TEST(QueryExecutor, RejectsDuplicateColumnNamesInCreateTableAndInsert) {
    DBMS dbms;
    execute_sql(dbms, "CREATE DATABASE app; USE app;");
    EXPECT_THROW(execute_sql(dbms, "CREATE TABLE users (id INT, id STRING);"), std::exception);

    execute_sql(dbms, "CREATE TABLE users (id INT, name STRING);");
    EXPECT_THROW(execute_sql(dbms, "INSERT INTO users (id, id) VALUE (1, 2);"), std::exception);
}

TEST(QueryExecutor, RejectsInsertValueCountMismatch) {
    DBMS dbms;
    execute_sql(dbms, "CREATE DATABASE app; USE app; CREATE TABLE users (id INT, name STRING);");
    EXPECT_THROW(execute_sql(dbms, "INSERT INTO users (id, name) VALUE (1);"), std::exception);
    EXPECT_THROW(execute_sql(dbms, "INSERT INTO users (id) VALUE (1, \"Ann\");"), std::exception);
}

TEST(QueryExecutor, RejectsConstraintViolations) {
    DBMS dbms;
    execute_sql(dbms, "CREATE DATABASE app; USE app; CREATE TABLE users (id INT INDEXED, name STRING NOT NULL);");

    EXPECT_THROW(execute_sql(dbms, "INSERT INTO users (id, name) VALUE (NULL, \"Ann\");"), std::exception);
    EXPECT_THROW(execute_sql(dbms, "INSERT INTO users (id) VALUE (1);"), std::exception);

    execute_sql(dbms, "INSERT INTO users (id, name) VALUE (1, \"Ann\");");
    EXPECT_THROW(execute_sql(dbms, "INSERT INTO users (id, name) VALUE (1, \"Bob\");"), std::exception);
}

TEST(QueryExecutor, RejectsInsertConstraintViolationsWithoutInsertingPartialBatch) {
    DBMS dbms;
    execute_sql(dbms, "CREATE DATABASE app; USE app; CREATE TABLE users (id INT INDEXED, name STRING NOT NULL);");

    EXPECT_THROW(
        execute_sql(dbms, "INSERT INTO users (id, name) VALUE (1, \"Ann\"), (1, \"Duplicate\");"),
        std::exception);

    const std::vector<std::string> results = execute_sql(dbms, "SELECT * FROM users;");
    ASSERT_EQ(results.size(), 1);
    expect_json_eq(results[0], R"([])");
}

TEST(QueryExecutor, RejectsTypeMismatchesAndUnknownColumns) {
    DBMS dbms;
    execute_sql(dbms, "CREATE DATABASE app; USE app; CREATE TABLE users (id INT, name STRING);");

    EXPECT_THROW(execute_sql(dbms, "INSERT INTO users (id, name) VALUE (\"bad\", \"Ann\");"), std::exception);
    EXPECT_THROW(execute_sql(dbms, "INSERT INTO users (missing) VALUE (1);"), std::exception);
    EXPECT_THROW(execute_sql(dbms, "SELECT missing FROM users;"), std::exception);
    EXPECT_THROW(execute_sql(dbms, "UPDATE users SET missing = 1;"), std::exception);
}

TEST(QueryExecutor, RejectsInvalidWhereOperations) {
    DBMS dbms;
    execute_sql(dbms, "CREATE DATABASE app; USE app; CREATE TABLE users (id INT, name STRING); INSERT INTO users (id, name) VALUE (1, \"Ann\");");

    EXPECT_THROW(execute_sql(dbms, "SELECT * FROM users WHERE id < \"abc\";"), std::exception);
    EXPECT_THROW(execute_sql(dbms, "SELECT * FROM users WHERE id LIKE \"1\";"), std::exception);
    EXPECT_THROW(execute_sql(dbms, "SELECT * FROM users WHERE id BETWEEN 1 AND \"bad\";"), std::exception);
}

TEST(QueryExecutor, HandlesEscapedStringLiterals) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE texts (value STRING);"
        "INSERT INTO texts (value) VALUE (\"line\\nquote\\\"slash\\\\\");"
        "SELECT * FROM texts;");

    ASSERT_EQ(results.size(), 5);
    expect_json_eq(results.back(), "[{\"value\":\"line\\nquote\\\"slash\\\\\"}]");
}

TEST(QueryExecutor, AcceptsLowercaseKeywordsAndCommentsEndToEnd) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "-- create db\n"
        "create database app;\n"
        "use app;\n"
        "-- schema\n"
        "create table users (id int indexed, name string not null);\n"
        "insert into users (id, name) value (1, \"Ann\");\n"
        "select * from users; -- final result\n");

    ASSERT_EQ(results.size(), 5);
    expect_json_eq(results.back(), R"([{"id":1,"name":"Ann"}])");
}

TEST(QueryExecutor, SupportsAllComparisonOperators) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE numbers (n INT);"
        "INSERT INTO numbers (n) VALUE (1), (2), (3);"
        "SELECT n FROM numbers WHERE n == 2;"
        "SELECT n FROM numbers WHERE n != 2;"
        "SELECT n FROM numbers WHERE n < 2;"
        "SELECT n FROM numbers WHERE n > 2;"
        "SELECT n FROM numbers WHERE n <= 2;"
        "SELECT n FROM numbers WHERE n >= 2;");

    ASSERT_EQ(results.size(), 10);
    expect_json_eq(results[4], R"([{"n":2}])");
    expect_json_eq(results[5], R"([{"n":1},{"n":3}])");
    expect_json_eq(results[6], R"([{"n":1}])");
    expect_json_eq(results[7], R"([{"n":3}])");
    expect_json_eq(results[8], R"([{"n":1},{"n":2}])");
    expect_json_eq(results[9], R"([{"n":2},{"n":3}])");
}

TEST(QueryExecutor, SupportsNullEqualityAndInequalityPredicates) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE users (id INT, name STRING);"
        "INSERT INTO users (id, name) VALUE (1, NULL), (2, \"Ann\");"
        "SELECT id FROM users WHERE name == NULL;"
        "SELECT id FROM users WHERE name != NULL;");

    ASSERT_EQ(results.size(), 6);
    expect_json_eq(results[4], R"([{"id":1}])");
    expect_json_eq(results[5], R"([{"id":2}])");
}

TEST(QueryExecutor, UpdateWithoutWhereUpdatesAllRowsAndNoMatchUpdatesZeroRows) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE users (id INT, active INT);"
        "INSERT INTO users (id, active) VALUE (1, 0), (2, 0);"
        "UPDATE users SET active = 1;"
        "UPDATE users SET active = 0 WHERE id == 99;"
        "SELECT * FROM users;");

    ASSERT_EQ(results.size(), 7);
    expect_json_eq(results[4], R"({"count":2,"operation":"update","status":"ok"})");
    expect_json_eq(results[5], R"({"count":0,"operation":"update","status":"ok"})");
    expect_json_eq(results[6], R"([{"active":1,"id":1},{"active":1,"id":2}])");
}

TEST(QueryExecutor, DeleteNoMatchDeletesZeroRowsAndIndexedValueCanBeReusedAfterDelete) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE users (id INT INDEXED, name STRING);"
        "INSERT INTO users (id, name) VALUE (1, \"Ann\"), (2, \"Bob\");"
        "DELETE FROM users WHERE id == 99;"
        "DELETE FROM users WHERE id == 1;"
        "INSERT INTO users (id, name) VALUE (1, \"Alice\");"
        "SELECT * FROM users;");

    ASSERT_EQ(results.size(), 8);
    expect_json_eq(results[4], R"({"count":0,"operation":"delete","status":"ok"})");
    expect_json_eq(results[5], R"({"count":1,"operation":"delete","status":"ok"})");
    expect_json_eq(results[7], R"([{"id":2,"name":"Bob"},{"id":1,"name":"Alice"}])");
}

TEST(QueryExecutor, RejectsUseDropAndSelectMissingObjects) {
    DBMS dbms;
    EXPECT_THROW(execute_sql(dbms, "USE missing;"), std::exception);
    EXPECT_THROW(execute_sql(dbms, "DROP DATABASE missing;"), std::exception);

    execute_sql(dbms, "CREATE DATABASE app; USE app;");
    EXPECT_THROW(execute_sql(dbms, "DROP TABLE missing;"), std::exception);
    EXPECT_THROW(execute_sql(dbms, "SELECT * FROM missing;"), std::exception);
    EXPECT_THROW(execute_sql(dbms, "CREATE TABLE other.users (id INT);"), std::exception);
}

TEST(QueryExecutor, RejectsDuplicateAssignments) {
    DBMS dbms;
    execute_sql(dbms, "CREATE DATABASE app; USE app; CREATE TABLE users (id INT, name STRING); INSERT INTO users (id, name) VALUE (1, \"Ann\");");
    EXPECT_THROW(execute_sql(dbms, "UPDATE users SET name = \"A\", name = \"B\" WHERE id == 1;"), std::exception);
}

TEST(QueryExecutor, RejectsUpdateConstraintViolationsWithoutChangingRows) {
    DBMS dbms;
    execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE users (id INT INDEXED, name STRING NOT NULL);"
        "INSERT INTO users (id, name) VALUE (1, \"Ann\"), (2, \"Bob\");");

    EXPECT_THROW(execute_sql(dbms, "UPDATE users SET name = NULL WHERE id == 1;"), std::exception);
    EXPECT_THROW(execute_sql(dbms, "UPDATE users SET id = 2 WHERE id == 1;"), std::exception);
    EXPECT_THROW(execute_sql(dbms, "UPDATE users SET id = \"bad\" WHERE id == 1;"), std::exception);

    const std::vector<std::string> results = execute_sql(dbms, "SELECT * FROM users;");
    ASSERT_EQ(results.size(), 1);
    expect_json_eq(results[0], R"([{"id":1,"name":"Ann"},{"id":2,"name":"Bob"}])");
}

TEST(QueryExecutor, RejectsUpdateConstraintViolationsWithoutChangingPartialBatch) {
    DBMS dbms;
    execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE users (id INT INDEXED, name STRING);"
        "INSERT INTO users (id, name) VALUE (1, \"Ann\"), (2, \"Bob\");");

    EXPECT_THROW(execute_sql(dbms, "UPDATE users SET id = 3;"), std::exception);

    const std::vector<std::string> results = execute_sql(dbms, "SELECT * FROM users;");
    ASSERT_EQ(results.size(), 1);
    expect_json_eq(results[0], R"([{"id":1,"name":"Ann"},{"id":2,"name":"Bob"}])");
}

TEST(QueryExecutor, RejectsOrderingNullsAndInvalidRegex) {
    DBMS dbms;
    execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE users (id INT, name STRING);"
        "INSERT INTO users (id, name) VALUE (1, \"Ann\"), (2, NULL);");

    EXPECT_THROW(execute_sql(dbms, "SELECT * FROM users WHERE name < \"Bob\";"), std::exception);
    try {
        execute_sql(dbms, "SELECT * FROM users WHERE name LIKE \"[\";");
        FAIL() << "Expected invalid LIKE regex to throw";
    } catch (const std::runtime_error &ex) {
        EXPECT_STREQ(ex.what(), "Invalid LIKE regex pattern");
    }
}

TEST(QueryExecutor, DoesNotPartiallyDeleteWhenWhereEvaluationFails) {
    DBMS dbms;
    execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE users (id INT, name STRING);"
        "INSERT INTO users (id, name) VALUE (1, \"Ann\"), (2, NULL);");

    EXPECT_THROW(execute_sql(dbms, "DELETE FROM users WHERE id == 1 OR name < \"Bob\";"), std::exception);

    const std::vector<std::string> results = execute_sql(dbms, "SELECT * FROM users;");
    ASSERT_EQ(results.size(), 1);
    expect_json_eq(results[0], R"([{"id":1,"name":"Ann"},{"id":2,"name":null}])");
}

TEST(QueryExecutor, EmptySelectFromEmptyTableReturnsEmptyArray) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE users (id INT, name STRING);"
        "SELECT * FROM users;"
        "SELECT id FROM users WHERE id == 1;");

    ASSERT_EQ(results.size(), 5);
    expect_json_eq(results[3], R"([])");
    expect_json_eq(results[4], R"([])");
}

TEST(QueryExecutor, SupportsStringBetweenHalfOpenInterval) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE words (word STRING);"
        "INSERT INTO words (word) VALUE (\"ant\"), (\"bee\"), (\"cat\"), (\"dog\");"
        "SELECT * FROM words WHERE word BETWEEN \"bee\" AND \"dog\";");

    ASSERT_EQ(results.size(), 5);
    expect_json_eq(results.back(), R"([{"word":"bee"},{"word":"cat"}])");
}

TEST(QueryExecutor, SupportsEscapedTabAndCarriageReturn) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE texts (s STRING);"
        "INSERT INTO texts (s) VALUE (\"a\\tb\\r\");"
        "SELECT * FROM texts;");

    ASSERT_EQ(results.size(), 5);
    expect_json_eq(results.back(), "[{\"s\":\"a\\tb\\r\"}]");
}

TEST(QueryExecutor, SupportsNegativeIntegerLiteralsInInsert) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE numbers (id INT, value INT);"
        "INSERT INTO numbers (id, value) VALUE (1, -10), (2, -5), (3, 0), (4, 5);"
        "SELECT * FROM numbers;");

    ASSERT_EQ(results.size(), 5);
    expect_json_eq(results.back(), R"([{"id":1,"value":-10},{"id":2,"value":-5},{"id":3,"value":0},{"id":4,"value":5}])");
}

TEST(QueryExecutor, SupportsNegativeNumbersInWhereConditions) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE numbers (value INT);"
        "INSERT INTO numbers (value) VALUE (-10), (-5), (0), (5), (10);"
        "SELECT * FROM numbers WHERE value == -5;"
        "SELECT * FROM numbers WHERE value < 0;"
        "SELECT * FROM numbers WHERE value > -10;");

    ASSERT_EQ(results.size(), 7);
    expect_json_eq(results[4], R"([{"value":-5}])");
    expect_json_eq(results[5], R"([{"value":-10},{"value":-5}])");
    expect_json_eq(results[6], R"([{"value":-5},{"value":0},{"value":5},{"value":10}])");
}

TEST(QueryExecutor, SupportsNegativeNumbersInBetween) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE numbers (value INT);"
        "INSERT INTO numbers (value) VALUE (-10), (-5), (0), (5), (10);"
        "SELECT * FROM numbers WHERE value BETWEEN -10 AND 0;");

    ASSERT_EQ(results.size(), 5);
    expect_json_eq(results.back(), R"([{"value":-10},{"value":-5}])");
}

TEST(QueryExecutor, SupportsNegativeNumbersInUpdate) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE numbers (id INT, value INT);"
        "INSERT INTO numbers (id, value) VALUE (1, 0), (2, 0);"
        "UPDATE numbers SET value = -100 WHERE id == 1;"
        "SELECT * FROM numbers;");

    ASSERT_EQ(results.size(), 6);
    expect_json_eq(results[4], R"({"count":1,"operation":"update","status":"ok"})");
    expect_json_eq(results[5], R"([{"id":1,"value":-100},{"id":2,"value":0}])");
}

TEST(QueryExecutor, SupportsMinimumIntegerLiteral) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE numbers (value INT);"
        "INSERT INTO numbers (value) VALUE (-2147483648);"
        "SELECT * FROM numbers WHERE value == -2147483648;");

    ASSERT_EQ(results.size(), 5);
    expect_json_eq(results.back(), R"([{"value":-2147483648}])");
}

TEST(QueryParser, RejectsPositiveIntegerLiteralOverflow) {
    EXPECT_THROW(
        parse_sql("CREATE TABLE numbers (value INT); INSERT INTO numbers (value) VALUE (2147483648);"),
        std::exception);
}

TEST(QueryExecutor, SupportsLikePatternExpression) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE rules (name STRING, pattern STRING);"
        "INSERT INTO rules (name, pattern) VALUE (\"Ann\", \"A.*\"), (\"Bob\", \"A.*\");"
        "SELECT name FROM rules WHERE name LIKE pattern;");

    ASSERT_EQ(results.size(), 5);
    expect_json_eq(results.back(), R"([{"name":"Ann"}])");
}

TEST(QueryExecutor, SupportsNegativeNumbersWithAllComparisonOperators) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE numbers (n INT);"
        "INSERT INTO numbers (n) VALUE (-2), (-1), (0), (1), (2);"
        "SELECT n FROM numbers WHERE n == -1;"
        "SELECT n FROM numbers WHERE n != -1;"
        "SELECT n FROM numbers WHERE n < -1;"
        "SELECT n FROM numbers WHERE n > -1;"
        "SELECT n FROM numbers WHERE n <= -1;"
        "SELECT n FROM numbers WHERE n >= -1;");

    ASSERT_EQ(results.size(), 10);
    expect_json_eq(results[4], R"([{"n":-1}])");
    expect_json_eq(results[5], R"([{"n":-2},{"n":0},{"n":1},{"n":2}])");
    expect_json_eq(results[6], R"([{"n":-2}])");
    expect_json_eq(results[7], R"([{"n":0},{"n":1},{"n":2}])");
    expect_json_eq(results[8], R"([{"n":-2},{"n":-1}])");
    expect_json_eq(results[9], R"([{"n":-1},{"n":0},{"n":1},{"n":2}])");
}


TEST(QueryExecutor, NegativeNumbersWithNullComparisons) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE mixed (id INT, value INT);"
        "INSERT INTO mixed (id, value) VALUE (1, -100), (2, NULL), (3, 100);"
        "SELECT * FROM mixed WHERE value == -100;"
        "SELECT * FROM mixed WHERE value == NULL;"
        "SELECT * FROM mixed WHERE value != NULL;");

    ASSERT_EQ(results.size(), 7);
    expect_json_eq(results[4], R"([{"id":1,"value":-100}])");
    expect_json_eq(results[5], R"([{"id":2,"value":null}])");
    expect_json_eq(results[6], R"([{"id":1,"value":-100},{"id":3,"value":100}])");
}

TEST(QueryExecutor, ComparisonsAroundNegativeOne) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE numbers (n INT);"
        "INSERT INTO numbers (n) VALUE (-3), (-2), (-1), (0), (1);"
        "SELECT * FROM numbers WHERE n > -1;"
        "SELECT * FROM numbers WHERE n >= -1;"
        "SELECT * FROM numbers WHERE n < -1;"
        "SELECT * FROM numbers WHERE n <= -1;");

    ASSERT_EQ(results.size(), 8);
    expect_json_eq(results[4], R"([{"n":0},{"n":1}])");
    expect_json_eq(results[5], R"([{"n":-1},{"n":0},{"n":1}])");
    expect_json_eq(results[6], R"([{"n":-3},{"n":-2}])");
    expect_json_eq(results[7], R"([{"n":-3},{"n":-2},{"n":-1}])");
}

TEST(QueryExecutor, UpdateFromPositiveToNegativeAndViceVersa) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE numbers (id INT, value INT);"
        "INSERT INTO numbers (id, value) VALUE (1, 100), (2, -100);"
        "UPDATE numbers SET value = -value WHERE id == 1;"
        "UPDATE numbers SET value = -value WHERE id == 2;"
        "SELECT * FROM numbers;");

    ASSERT_EQ(results.size(), 7);
    expect_json_eq(results[5], R"({"count":1,"operation":"update","status":"ok"})");
    expect_json_eq(results[6], R"([{"id":1,"value":-100},{"id":2,"value":100}])");
}

TEST(QueryExecutor, RejectsUnaryMinusOverflow) {
    DBMS dbms;
    execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE numbers (id INT, value INT);"
        "INSERT INTO numbers (id, value) VALUE (1, -2147483648);");

    EXPECT_THROW(execute_sql(dbms, "UPDATE numbers SET value = -value WHERE id == 1;"), std::runtime_error);
}


TEST(QueryExecutor, SelectWithComplexNegativeConditions) {
    DBMS dbms;
    const std::vector<std::string> results = execute_sql(
        dbms,
        "CREATE DATABASE app;"
        "USE app;"
        "CREATE TABLE numbers (n INT, m INT);"
        "INSERT INTO numbers (n, m) VALUE (-10, -2147483647), (-5, -10), (0, 0), (5, -5), (10, 5);"
        "SELECT * FROM numbers WHERE n < 0 AND m < 0;"
        "SELECT * FROM numbers WHERE n > -10 OR m > -5;");

    ASSERT_EQ(results.size(), 6);
    expect_json_eq(results[4], R"([{"n":-10,"m":-2147483647},{"n":-5,"m":-10}])");
    expect_json_eq(results[5], R"([{"n":-5,"m":-10},{"n":0,"m":0},{"n":5,"m":-5},{"n":10,"m":5}])");
}
