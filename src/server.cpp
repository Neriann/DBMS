#include "crow.h"
#include "core/DBMS.hpp"
#include "query/Scanner.hpp"
#include "parser.hpp"
#include "query/Executor.hpp"
#include "storage/StorageManager.hpp"

#include <sstream>
#include <string>
#include <vector>

static std::vector<Statement> parse_sql(const std::string &sql) {
    std::istringstream in(sql);
    Scanner scanner(in);
    std::vector<Statement> stmts;
    yy::Parser parser(scanner, stmts);
    parser.parse();
    return stmts;
}

int main() {
    DBMS dbms;
    StorageManager storage("./data");
    storage.load(dbms);

    Executor exec(dbms);

    crow::SimpleApp app;

    CROW_ROUTE(app, "/query").methods(crow::HTTPMethod::Post)(
        [&exec](const crow::request &req) {
            try {
                std::string output;
                for (const Statement &stmt: parse_sql(req.body)) {
                    std::string r = exec.execute(stmt);
                    if (!r.empty()) {
                        if (!output.empty()) output += '\n';
                        output += r;
                    }
                }
                return crow::response(200, output.empty() ? "OK" : output);
            } catch (const std::exception &e) {
                return crow::response(400, e.what());
            }
        });

    app.port(8080).multithreaded().run();
}
