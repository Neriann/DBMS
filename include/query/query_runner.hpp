#pragma once

#include "query/executor.hpp"

#include <string>
#include <vector>

std::vector<Statement> parse_sql(const std::string &sql);
std::string run_statements(const std::vector<Statement> &statements, Executor &exec);

/**
 *
 * @param sql SQL source text
 * @param exec Executor
 * @return Concatenated non-empty statement results
 */
std::string run_sql(const std::string &sql, Executor &exec);
