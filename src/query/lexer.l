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

{blank}             { throw not_implemented("yy::Parser::symbol_type Scanner::next_token()", "is not implemented"); }
{comment}           { throw not_implemented("yy::Parser::symbol_type Scanner::next_token()", "is not implemented"); }
{string}            { throw not_implemented("yy::Parser::symbol_type Scanner::next_token()", "is not implemented"); }
{number}            { throw not_implemented("yy::Parser::symbol_type Scanner::next_token()", "is not implemented"); }
{identifier}        { throw not_implemented("yy::Parser::symbol_type Scanner::next_token()", "is not implemented"); }

"=="    { throw not_implemented("yy::Parser::symbol_type Scanner::next_token()", "is not implemented"); }
"!="    { throw not_implemented("yy::Parser::symbol_type Scanner::next_token()", "is not implemented"); }
"<="    { throw not_implemented("yy::Parser::symbol_type Scanner::next_token()", "is not implemented"); }
">="    { throw not_implemented("yy::Parser::symbol_type Scanner::next_token()", "is not implemented"); }
"<"     { throw not_implemented("yy::Parser::symbol_type Scanner::next_token()", "is not implemented"); }
">"     { throw not_implemented("yy::Parser::symbol_type Scanner::next_token()", "is not implemented"); }
"="     { throw not_implemented("yy::Parser::symbol_type Scanner::next_token()", "is not implemented"); }
"("     { throw not_implemented("yy::Parser::symbol_type Scanner::next_token()", "is not implemented"); }
")"     { throw not_implemented("yy::Parser::symbol_type Scanner::next_token()", "is not implemented"); }
","     { throw not_implemented("yy::Parser::symbol_type Scanner::next_token()", "is not implemented"); }
";"     { throw not_implemented("yy::Parser::symbol_type Scanner::next_token()", "is not implemented"); }
"."     { throw not_implemented("yy::Parser::symbol_type Scanner::next_token()", "is not implemented"); }
"*"     { throw not_implemented("yy::Parser::symbol_type Scanner::next_token()", "is not implemented"); }

<<EOF>> { throw not_implemented("yy::Parser::symbol_type Scanner::next_token()", "is not implemented"); }
.       { throw not_implemented("yy::Parser::symbol_type Scanner::next_token()", "is not implemented"); }

%%
