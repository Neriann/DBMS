#pragma once

#include "query/executor.hpp"

#include <string>
#include <vector>

/**
 * @param sql SQL source text containing zero or more semicolon-terminated statements
 * @return parsed statement list
 * @throws std::exception on lexical or syntax errors
 */
std::vector<Statement> parse_sql(const std::string &sql);

/**
 * Executes already parsed statements in order.
 *
 * @param statements parsed statement list
 * @param exec executor bound to a DBMS instance
 * @return concatenated non-empty statement results separated by newlines
 */
std::string run_statements(const std::vector<Statement> &statements, Executor &exec);

/**
 * Parses and executes SQL source text.
 *
 * @param sql SQL source text
 * @param exec executor bound to a DBMS instance
 * @return concatenated non-empty statement results separated by newlines
 */
std::string run_sql(const std::string &sql, Executor &exec);
