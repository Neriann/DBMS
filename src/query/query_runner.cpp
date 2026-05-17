#include "query/query_runner.hpp"

#include "parser.hpp"
#include "query/scanner.hpp"

#include <sstream>
#include <vector>

namespace {
    std::vector<Statement> parse_sql(const std::string &sql) {
        std::istringstream in(sql);
        Scanner scanner(in);
        std::vector<Statement> stmts;
        yy::Parser parser(scanner, stmts);
        parser.parse();
        return stmts;
    }
} // namespace

std::string run_sql(const std::string &sql, Executor &exec) {
    std::string output;

    for (const Statement &stmt: parse_sql(sql)) {
        if (auto result = exec.execute(stmt); !result.empty()) {
            if (!output.empty()) output += '\n';
            output += result;
        }
    }

    return output;
}
