%{
#include "query/scanner.hpp"
#include "not_implemented.h"
#include <algorithm>
#include <stdexcept>

#undef  YY_DECL
#define YY_DECL yy::Parser::symbol_type Scanner::next_token()
%}

%option c++
%option yyclass="Scanner"

%option noyywrap nounput noinput

blank       [ \t\r\n]+
comment     "--"[^\n]*
string      \"([^\"\\]|\\[\"\\nrt])*\"
number      [0-9]+
identifier  [a-zA-Z_][a-zA-Z0-9_]*

%%

{blank}             { /* skip */ }
{comment}           { /* skip */ }
{string}            { return yy::Parser::make_STRING_LIT(unquote_string(yytext, yyleng)); }
{number}            { return yy::Parser::make_INT_LIT(parse_int_literal(yytext)); }
{identifier}        { throw not_implemented("yy::Parser::symbol_type Scanner::next_token()", "is not implemented"); }

"=="    { return yy::Parser::make_EQ(); }
"!="    { return yy::Parser::make_NEQ(); }
"<="    { return yy::Parser::make_LEQ(); }
">="    { return yy::Parser::make_GEQ(); }
"<"     { return yy::Parser::make_LT(); }
">"     { return yy::Parser::make_GT(); }
"="     { return yy::Parser::make_ASSIGN(); }
"("     { return yy::Parser::make_LPAREN(); }
")"     { return yy::Parser::make_RPAREN(); }
","     { return yy::Parser::make_COMMA(); }
";"     { return yy::Parser::make_SEMICOLON(); }
"."     { return yy::Parser::make_DOT(); }
"*"     { return yy::Parser::make_STAR(); }

<<EOF>> { return yy::Parser::make_YYEOF(); }
.       { throw std::runtime_error("invalid character: " + std::string(yytext, yyleng)); }

%%
