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
    #include "not_implemented.h"

    // Bison calls yylex(scanner), and the call is forwarded to the real C++ scanner method.
    static yy::Parser::symbol_type yylex(Scanner &scanner) {
        return scanner.next_token();
    }
}

%token <std::string> IDENT
%token <std::string> STRING_LIT
%token <int> INT_LIT
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

%left KW_OR
%left KW_AND

%type <Statement> statement

%type <Expr> expr

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
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }

    | KW_DROP KW_DATABASE ident SEMICOLON
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }

    | KW_USE ident SEMICOLON
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }

    | KW_CREATE KW_TABLE table_ref LPAREN column_defs RPAREN SEMICOLON
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }

    | KW_DROP KW_TABLE table_ref SEMICOLON
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }

    | KW_INSERT KW_INTO table_ref LPAREN ident_list RPAREN KW_VALUE value_tuples SEMICOLON
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }

    | KW_UPDATE table_ref KW_SET assignment_list opt_where SEMICOLON
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }

    | KW_DELETE KW_FROM table_ref opt_where SEMICOLON
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }

    | KW_SELECT STAR KW_FROM table_ref opt_where SEMICOLON
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }

    | KW_SELECT select_cols KW_FROM table_ref opt_where SEMICOLON
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }
    ;
/* endregion Statements */

/* region Names */

ident:
      IDENT
        {
            $$ = std::move($1);
        }
    ;

table_ref:
      ident DOT ident
        {
            $$ = { std::move($1), std::move($3) };
        }

    | ident
        {
            $$ = { "", std::move($1) };
        }
    ;

column_defs:
      column_def
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }

    | column_defs COMMA column_def
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }
    ;

column_def:
      ident col_type constraints
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }
    ;

col_type:
      KW_INT
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }

    | KW_STRING
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }
    ;

constraints:
      %empty
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }

    | constraints KW_NOT_NULL
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }

    | constraints KW_INDEXED
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }
    ;

ident_list:
      ident
        {
            $$ = std::vector<std::string>{ std::move($1) };
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
            $$ = std::vector<Row>{ std::move($1) };
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
      expr
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }

    | value_list COMMA expr
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }
    ;

assignment_list:
      assignment
        {
            $$ = std::vector<std::pair<std::string, Expr>>{ std::move($1) };
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
            $$ = { std::move($1), std::move($3) };
        }
    ;

select_cols:
      select_col
        {
            $$ = std::vector<SelectColumn>{ std::move($1) };
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
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }

    | ident KW_AS ident
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
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
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }
    ;

and_cond:
      primary_cond
        {
            $$ = std::move($1);
        }

    | and_cond KW_AND primary_cond
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }
    ;

primary_cond:
      LPAREN condition RPAREN
        {
            $$ = std::move($2);
        }

    | expr KW_BETWEEN expr KW_AND expr
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }

    | expr KW_LIKE STRING_LIT
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }

    | expr cmp_op expr
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }
    ;

cmp_op:
      EQ
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }

    | NEQ
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }

    | LT
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }

    | GT
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }

    | LEQ
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }

    | GEQ
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }
    ;

expr:
      INT_LIT
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }

    | STRING_LIT
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }

    | NULL_LIT
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }

    | IDENT
        {
            throw not_implemented("yy::Parser::parse()", "is not implemented");
        }
    ;
%%

namespace yy {

void Parser::error(const std::string &msg) {
    throw std::runtime_error("parse error: " + msg);
}

} // namespace yy