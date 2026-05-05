%{
#include "query/Scanner.hpp"
#include "not_implemented.h"
#include <algorithm>
#include <stdexcept>

// Define next_token() instead of yylex() to avoid conflict with virtual int yyFlexLexer::yylex().
#undef  YY_DECL
#define YY_DECL yy::Parser::symbol_type Scanner::next_token()
%}

%option c++ noyywrap nounput noinput
%option yyclass="Scanner"

%%

[ \t\r\n]+          { throw not_implemented("yy::Parser::symbol_type Scanner::next_token()", "is not implemented"); }
"--"[^\n]*          { throw not_implemented("yy::Parser::symbol_type Scanner::next_token()", "is not implemented"); }
\"([^\"\\]|\\.)*\"  { throw not_implemented("yy::Parser::symbol_type Scanner::next_token()", "is not implemented"); }
[0-9]+              { throw not_implemented("yy::Parser::symbol_type Scanner::next_token()", "is not implemented"); }
[a-zA-Z_][a-zA-Z0-9_]* { throw not_implemented("yy::Parser::symbol_type Scanner::next_token()", "is not implemented"); }

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
