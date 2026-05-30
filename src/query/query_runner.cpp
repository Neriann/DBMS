#include "query/query_runner.hpp"

#include "parser.hpp"
#include "query/scanner.hpp"

#include <sstream>
#include <vector>

std::vector<Statement> parse_sql(const std::string &sql) {
    std::istringstream in(sql);
    Scanner scanner(in);
    std::vector<Statement> stmts;
    yy::Parser parser(scanner, stmts);
    parser.parse();
    return stmts;
}

std::string run_statements(const std::vector<Statement> &statements, Executor &exec) {
    std::string output;

    for (const Statement &stmt: statements) {
        if (auto result = exec.execute(stmt); !result.empty()) {
            if (!output.empty()) output += '\n';
            output += result;
        }
    }

    return output;
}

std::string run_sql(const std::string &sql, Executor &exec) {
    return run_statements(parse_sql(sql), exec);
}
