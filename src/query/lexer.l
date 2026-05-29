%{
#include "query/scanner.hpp"

#include <cctype>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>

static bool has_uniform_case(const std::string& text) {
    std::uint8_t flags = 0;

    constexpr std::uint8_t LOWER = 1 << 0;
    constexpr std::uint8_t UPPER = 1 << 1;

    for (unsigned char ch : text) {
        if (std::islower(ch)) {
            flags |= LOWER;
        } else if (std::isupper(ch)) {
            flags |= UPPER;
        }

        if (flags == (LOWER | UPPER)) {
            return false;
        }
    }

    return true;
}

static std::string to_lower(std::string text) {
    for (char &ch : text) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return text;
}

static std::string unquote_string(const char *text, std::size_t len) {
    std::string result;
    result.reserve(len > 1 ? len - 2 : 0);
    for (std::size_t i = 1; i + 1 < len; ++i) {
        if (text[i] != '\\') {
            result.push_back(text[i]);
            continue;
        }

        if (i + 2 >= len) {
            throw std::runtime_error("invalid string literal escape");
        }

        const char escaped = text[++i];
        switch (escaped) {
            case '"':
            case '\\':
                result.push_back(escaped);
                break;
            case 'n':
                result.push_back('\n');
                break;
            case 't':
                result.push_back('\t');
                break;
            case 'r':
                result.push_back('\r');
                break;
            default:
                throw std::runtime_error(std::string("unsupported escape sequence: \\") + escaped);
        }
    }
    return result;
}

#undef  YY_DECL
#define YY_DECL yy::Parser::symbol_type Scanner::next_token()
%}

%option c++
%option yyclass="Scanner"

%option noyywrap nounput noinput

blank       [ \t\r\n]+
comment     "--"[^\n]*
string      \"([^\"\\]|\\.)*\"
timestamp   [0-9]{4}"."[0-9]{2}"."[0-9]{2}"-"[0-9]{2}":"[0-9]{2}":"[0-9]{2}"."[0-9]{3,6}
number      0|[1-9][0-9]*
identifier  [a-zA-Z_][a-zA-Z0-9_]*

%%

{blank}             { /* skip */ }
{comment}           { /* skip */ }
{string}            { return yy::Parser::make_STRING_LIT(unquote_string(yytext, yyleng)); }
{timestamp}         { return yy::Parser::make_TIMESTAMP_LIT(std::string(yytext, yyleng)); }
{number}            { return yy::Parser::make_INT_LIT(std::string(yytext, yyleng)); }
{identifier}        {
                        const std::string text(yytext, yyleng);
                        const std::string lower = to_lower(text);

                        static const std::unordered_map<std::string, yy::Parser::token_kind_type> keywords = {
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
                            {"sum", yy::Parser::token::KW_SUM},
                            {"count", yy::Parser::token::KW_COUNT},
                            {"avg", yy::Parser::token::KW_AVG},
                            {"where", yy::Parser::token::KW_WHERE},
                            {"as", yy::Parser::token::KW_AS},
                            {"not", yy::Parser::token::KW_NOT},
                            {"not_null", yy::Parser::token::KW_NOT_NULL},
                            {"indexed", yy::Parser::token::KW_INDEXED},
                            {"default", yy::Parser::token::KW_DEFAULT},
                            {"and", yy::Parser::token::KW_AND},
                            {"or", yy::Parser::token::KW_OR},
                            {"between", yy::Parser::token::KW_BETWEEN},
                            {"like", yy::Parser::token::KW_LIKE},
                            {"int", yy::Parser::token::KW_INT},
                            {"string", yy::Parser::token::KW_STRING},
                            {"revert", yy::Parser::token::KW_REVERT},
                            {"null", yy::Parser::token::NULL_LIT}
                        };

                        const auto it = keywords.find(lower);
                        if (it == keywords.end()) {
                            return yy::Parser::make_IDENT(text);
                        }

                        if (!has_uniform_case(text)) {
                            throw std::runtime_error("mixed-case keyword not allowed: " + text);
                        }

                        return yy::Parser::symbol_type(it->second);
                    }

"=="    { return yy::Parser::make_EQ(); }
"!="    { return yy::Parser::make_NEQ(); }
"<="    { return yy::Parser::make_LEQ(); }
">="    { return yy::Parser::make_GEQ(); }
"<"     { return yy::Parser::make_LT(); }
">"     { return yy::Parser::make_GT(); }
"="     { return yy::Parser::make_ASSIGN(); }
"-"     { return yy::Parser::make_NEG(); }
"("     { return yy::Parser::make_LPAREN(); }
")"     { return yy::Parser::make_RPAREN(); }
","     { return yy::Parser::make_COMMA(); }
";"     { return yy::Parser::make_SEMICOLON(); }
"."     { return yy::Parser::make_DOT(); }
"*"     { return yy::Parser::make_STAR(); }

<<EOF>> { return yy::Parser::make_YYEOF(); }
.       { throw std::runtime_error("invalid character: " + std::string(yytext, yyleng)); }

%%
