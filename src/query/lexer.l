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
{identifier}        {
                        const std::string text(yytext, yyleng);
                        const std::string lower = to_lower(text);

                        static const std::unordered_map<std::string, int> keywords = {
                            {"create", yy::Parser::token::KW_CREATE},
                            {"drop", yy::Parser::token::KW_DROP},
                            {"use", yy::Parser::token::KW_USE},
                            {"database", yy::Parser::token::KW_DATABASE},
                            {"table", yy::Parser::token::KW_TABLE},
                            {"insert", yy::Parser::token::KW_INSERT},
                            {"into", yy::Parser::token::KW_INTO},
                            {"value", yy::Parser::token::KW_VALUE},
                            {"update", yy::Parser::token::KW_UPDATE},
                            {"set", yy::Parser::token::KW_SET},
                            {"delete", yy::Parser::token::KW_DELETE},
                            {"from", yy::Parser::token::KW_FROM},
                            {"select", yy::Parser::token::KW_SELECT},
                            {"where", yy::Parser::token::KW_WHERE},
                            {"as", yy::Parser::token::KW_AS},
                            {"not", yy::Parser::token::KW_NOT},
                            {"not_null", yy::Parser::token::KW_NOT_NULL},
                            {"indexed", yy::Parser::token::KW_INDEXED},
                            {"and", yy::Parser::token::KW_AND},
                            {"or", yy::Parser::token::KW_OR},
                            {"between", yy::Parser::token::KW_BETWEEN},
                            {"like", yy::Parser::token::KW_LIKE},
                            {"int", yy::Parser::token::KW_INT},
                            {"string", yy::Parser::token::KW_STRING},
                            {"null", yy::Parser::token::NULL_LIT}
                        };

                        const auto it = keywords.find(lower);
                        if (it == keywords.end()) {
                            auto result = yy::Parser::make_IDENT(text);
                            return result;
                        }

                        if (!is_all_upper_or_underscore(text) && !is_all_lower_or_underscore(text)) {
                            throw std::runtime_error("mixed-case keyword not allowed: " + text);
                        }

                        yy::Parser::symbol_type result = [&]() {
                            switch (it->second) {
                                case yy::Parser::token::KW_CREATE: return yy::Parser::make_KW_CREATE();
                                case yy::Parser::token::KW_DROP: return yy::Parser::make_KW_DROP();
                                case yy::Parser::token::KW_USE: return yy::Parser::make_KW_USE();
                                case yy::Parser::token::KW_DATABASE: return yy::Parser::make_KW_DATABASE();
                                case yy::Parser::token::KW_TABLE: return yy::Parser::make_KW_TABLE();
                                case yy::Parser::token::KW_INSERT: return yy::Parser::make_KW_INSERT();
                                case yy::Parser::token::KW_INTO: return yy::Parser::make_KW_INTO();
                                case yy::Parser::token::KW_VALUE: return yy::Parser::make_KW_VALUE();
                                case yy::Parser::token::KW_UPDATE: return yy::Parser::make_KW_UPDATE();
                                case yy::Parser::token::KW_SET: return yy::Parser::make_KW_SET();
                                case yy::Parser::token::KW_DELETE: return yy::Parser::make_KW_DELETE();
                                case yy::Parser::token::KW_FROM: return yy::Parser::make_KW_FROM();
                                case yy::Parser::token::KW_SELECT: return yy::Parser::make_KW_SELECT();
                                case yy::Parser::token::KW_WHERE: return yy::Parser::make_KW_WHERE();
                                case yy::Parser::token::KW_AS: return yy::Parser::make_KW_AS();
                                case yy::Parser::token::KW_NOT: return yy::Parser::make_KW_NOT();
                                case yy::Parser::token::KW_NOT_NULL: return yy::Parser::make_KW_NOT_NULL();
                                case yy::Parser::token::KW_INDEXED: return yy::Parser::make_KW_INDEXED();
                                case yy::Parser::token::KW_AND: return yy::Parser::make_KW_AND();
                                case yy::Parser::token::KW_OR: return yy::Parser::make_KW_OR();
                                case yy::Parser::token::KW_BETWEEN: return yy::Parser::make_KW_BETWEEN();
                                case yy::Parser::token::KW_LIKE: return yy::Parser::make_KW_LIKE();
                                case yy::Parser::token::KW_INT: return yy::Parser::make_KW_INT();
                                case yy::Parser::token::KW_STRING: return yy::Parser::make_KW_STRING();
                                case yy::Parser::token::NULL_LIT: return yy::Parser::make_NULL_LIT();
                                default: throw std::runtime_error("internal lexer error");
                            }
                        }();

                        return result;
                    }

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
