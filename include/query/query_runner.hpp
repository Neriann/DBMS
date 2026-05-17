#pragma once

#include "query/executor.hpp"

#include <string>

/**
 *
 * @param sql SQL source text
 * @param exec Executor
 * @return Concatenated non-empty statement results
 */
std::string run_sql(const std::string &sql, Executor &exec);
