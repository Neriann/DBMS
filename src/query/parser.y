%require "3.0"
%language "c++"

%define api.parser.class { Parser }
%define api.token.constructor
%define api.value.type variant
%define parse.error detailed

%code requires {
    #include "query/ast.hpp"

    class Scanner;
}

%lex-param { Scanner &scanner }

%parse-param { Scanner &scanner }
%parse-param { std::vector<Statement> &result }

%code {
    #include "query/scanner.hpp"

    #include <charconv>
    #include <stdexcept>
    #include <string>

    static yy::Parser::symbol_type yylex(Scanner &scanner) {
        return scanner.next_token();
    }

    static Value make_int_literal(const std::string &digits, bool negative) {
        const std::string text = negative ? "-" + digits : digits;

        int value = 0;
        const char *begin = text.data();
        const char *end = begin + text.size();
        const auto [ptr, ec] = std::from_chars(begin, end, value);

        if (ec == std::errc::result_out_of_range) {
            throw std::runtime_error("integer literal out of range");
        }
        if (ec != std::errc{} || ptr != end) {
            throw std::runtime_error("invalid integer literal: " + text);
        }

        return Value{value};
    }
}

%token <std::string> IDENT
%token <std::string> STRING_LIT
%token <std::string> INT_LIT
%token NULL_LIT

%token KW_CREATE
%token KW_DROP
%token KW_USE
%token KW_DATABASE
%token KW_TABLE
%token KW_INSERT
%token KW_INTO
%token KW_VALUE
%token KW_UPDATE
%token KW_SET
%token KW_DELETE
%token KW_FROM
%token KW_SELECT
%token KW_WHERE
%token KW_AS
%token KW_NOT
%token KW_NOT_NULL
%token KW_INDEXED
%token KW_AND
%token KW_OR
%token KW_BETWEEN
%token KW_LIKE
%token KW_INT
%token KW_STRING

%token LPAREN
%token RPAREN
%token COMMA
%token SEMICOLON
%token DOT
%token STAR
%token EQ
%token NEQ
%token LT
%token GT
%token LEQ
%token GEQ
%token ASSIGN
%token NEG

%left KW_OR
%left KW_AND

%type <Statement> statement

%type <Expr> expr
%type <Value> literal_value
%type <Value> int_literal_value

%type <Condition> condition
%type <Condition> or_cond
%type <Condition> and_cond
%type <Condition> primary_cond

%type <CmpOp> cmp_op

%type <std::string> ident

%type <std::pair<std::string, std::string>> table_ref

%type <Schema> column_defs
%type <Column> column_def
%type <ColumnType> col_type
%type <uint8_t> constraints

%type <std::vector<std::string>> ident_list

%type <Row> value_tuple
%type <Row> value_list
%type <std::vector<Row>> value_tuples

%type <std::pair<std::string, Expr>> assignment
%type <std::vector<std::pair<std::string, Expr>>> assignment_list

%type <SelectColumn> select_col
%type <std::vector<SelectColumn>> select_cols
%type <std::vector<SelectColumn>> select_list

%type <std::optional<Condition>> opt_where

%%
program:
      %empty
    | program statement
        {
            result.push_back(std::move($2));
        }
    ;

statement:
      KW_CREATE KW_DATABASE ident SEMICOLON
        {
            $$ = CreateDatabaseStmt{std::move($3)};
        }

    | KW_DROP KW_DATABASE ident SEMICOLON
        {
            $$ = DropDatabaseStmt{std::move($3)};
        }

    | KW_USE ident SEMICOLON
        {
            $$ = UseStmt{std::move($2)};
        }

    | KW_CREATE KW_TABLE table_ref LPAREN column_defs RPAREN SEMICOLON
        {
            $$ = CreateTableStmt{std::move($3.first), std::move($3.second), std::move($5)};
        }

    | KW_DROP KW_TABLE table_ref SEMICOLON
        {
            $$ = DropTableStmt{std::move($3.first), std::move($3.second)};
        }

    | KW_INSERT KW_INTO table_ref LPAREN ident_list RPAREN KW_VALUE value_tuples SEMICOLON
        {
            $$ = InsertStmt{std::move($3.first), std::move($3.second), std::move($5), std::move($8)};
        }

    | KW_UPDATE table_ref KW_SET assignment_list opt_where SEMICOLON
        {
            $$ = UpdateStmt{std::move($2.first), std::move($2.second), std::move($4), std::move($5)};
        }

    | KW_DELETE KW_FROM table_ref opt_where SEMICOLON
        {
            $$ = DeleteStmt{std::move($3.first), std::move($3.second), std::move($4)};
        }

    | KW_SELECT STAR KW_FROM table_ref opt_where SEMICOLON
        {
            $$ = SelectStmt{std::move($4.first), std::move($4.second), true, {}, std::move($5)};
        }

    | KW_SELECT select_list KW_FROM table_ref opt_where SEMICOLON
        {
            $$ = SelectStmt{std::move($4.first), std::move($4.second), false, std::move($2), std::move($5)};
        }
    ;

ident:
      IDENT
        {
            $$ = std::move($1);
        }

    | KW_VALUE
        {
            $$ = "value";
        }
    ;

table_ref:
      ident DOT ident
        {
            $$ = {std::move($1), std::move($3)};
        }

    | ident
        {
            $$ = {"", std::move($1)};
        }
    ;

column_defs:
      column_def
        {
            $$ = Schema{std::move($1)};
        }

    | column_defs COMMA column_def
        {
            $1.push_back(std::move($3));
            $$ = std::move($1);
        }
    ;

column_def:
      ident col_type constraints
        {
            $$ = Column{std::move($1), $2, $3};
        }
    ;

col_type:
      KW_INT
        {
            $$ = ColumnType::INT;
        }

    | KW_STRING
        {
            $$ = ColumnType::STRING;
        }
    ;

constraints:
      %empty
        {
            $$ = NONE;
        }

    | constraints KW_NOT_NULL
        {
            $$ = static_cast<std::uint8_t>($1 | NOT_NULL);
        }

    | constraints KW_NOT NULL_LIT
        {
            $$ = static_cast<std::uint8_t>($1 | NOT_NULL);
        }

    | constraints KW_INDEXED
        {
            $$ = static_cast<std::uint8_t>($1 | INDEXED);
        }
    ;

ident_list:
      ident
        {
            $$ = std::vector<std::string>{std::move($1)};
        }

    | ident_list COMMA ident
        {
            $1.push_back(std::move($3));
            $$ = std::move($1);
        }
    ;

value_tuples:
      value_tuple
        {
            $$ = std::vector<Row>{std::move($1)};
        }

    | value_tuples COMMA value_tuple
        {
            $1.push_back(std::move($3));
            $$ = std::move($1);
        }
    ;

value_tuple:
      LPAREN value_list RPAREN
        {
            $$ = std::move($2);
        }
    ;

value_list:
      literal_value
        {
            $$ = Row{std::move($1)};
        }

    | value_list COMMA literal_value
        {
            $1.push_back(std::move($3));
            $$ = std::move($1);
        }
    ;

assignment_list:
      assignment
        {
            $$ = std::vector<std::pair<std::string, Expr>>{std::move($1)};
        }

    | assignment_list COMMA assignment
        {
            $1.push_back(std::move($3));
            $$ = std::move($1);
        }
    ;

assignment:
      ident ASSIGN expr
        {
            $$ = {std::move($1), std::move($3)};
        }
    ;

select_cols:
      select_col
        {
            $$ = std::vector<SelectColumn>{std::move($1)};
        }

    | select_cols COMMA select_col
        {
            $1.push_back(std::move($3));
            $$ = std::move($1);
        }
    ;

select_col:
      ident
        {
            $$ = SelectColumn{std::move($1), ""};
        }

    | ident KW_AS ident
        {
            $$ = SelectColumn{std::move($1), std::move($3)};
        }
    ;

select_list:
      select_cols
        {
            $$ = std::move($1);
        }

    | LPAREN select_cols RPAREN
        {
            $$ = std::move($2);
        }
    ;

opt_where:
      %empty
        {
            $$ = std::nullopt;
        }

    | KW_WHERE condition
        {
            $$ = std::move($2);
        }
    ;

condition:
      or_cond
        {
            $$ = std::move($1);
        }
    ;

or_cond:
      and_cond
        {
            $$ = std::move($1);
        }

    | or_cond KW_OR and_cond
        {
            Condition cond;
            cond.kind = ConditionKind::Or;
            cond.left = std::make_shared<Condition>(std::move($1));
            cond.right = std::make_shared<Condition>(std::move($3));
            $$ = std::move(cond);
        }
    ;

and_cond:
      primary_cond
        {
            $$ = std::move($1);
        }

    | and_cond KW_AND primary_cond
        {
            Condition cond;
            cond.kind = ConditionKind::And;
            cond.left = std::make_shared<Condition>(std::move($1));
            cond.right = std::make_shared<Condition>(std::move($3));
            $$ = std::move(cond);
        }
    ;

primary_cond:
      LPAREN condition RPAREN
        {
            $$ = std::move($2);
        }

    | expr KW_BETWEEN expr KW_AND expr
        {
            Condition cond;
            cond.kind = ConditionKind::Between;
            cond.between = BetweenPredicate{std::move($1), std::move($3), std::move($5)};
            $$ = std::move(cond);
        }

    | expr KW_LIKE STRING_LIT
        {
            Condition cond;
            cond.kind = ConditionKind::Like;
            cond.like = LikePredicate{std::move($1), std::move($3)};
            $$ = std::move(cond);
        }

    | expr cmp_op expr
        {
            Condition cond;
            cond.kind = ConditionKind::Simple;
            cond.predicate = Predicate{std::move($1), $2, std::move($3)};
            $$ = std::move(cond);
        }
    ;

cmp_op:
      EQ { $$ = CmpOp::EQ; }
    | NEQ { $$ = CmpOp::NEQ; }
    | LT { $$ = CmpOp::LT; }
    | GT { $$ = CmpOp::GT; }
    | LEQ { $$ = CmpOp::LEQ; }
    | GEQ { $$ = CmpOp::GEQ; }
    ;

expr:
      literal_value
        {
            $$ = Expr{ExprKind::Literal, std::move($1), "", nullptr};
        }

    | ident
        {
            $$ = Expr{ExprKind::Column, nullptr, std::move($1), nullptr};
        }

    | NEG ident
        {
            Expr operand{ExprKind::Column, nullptr, std::move($2), nullptr};
            $$ = Expr{ExprKind::UnaryMinus, nullptr, "", std::make_shared<Expr>(std::move(operand))};
        }
    ;

literal_value:
      int_literal_value
        {
            $$ = std::move($1);
        }

    | STRING_LIT
        {
            $$ = Value{std::move($1)};
        }

    | NULL_LIT
        {
            $$ = Value{nullptr};
        }
    ;

int_literal_value:
      INT_LIT
        {
            $$ = make_int_literal($1, false);
        }

    | NEG INT_LIT
        {
            $$ = make_int_literal($2, true);
        }
    ;

%%

namespace yy {

void Parser::error(const std::string &msg) {
    throw std::runtime_error("parse error: " + msg);
}

} // namespace yy
