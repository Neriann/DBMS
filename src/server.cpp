#include "crow.h"
#include "core/dbms.hpp"
#include "query/scanner.hpp"
#include "parser.hpp"
#include "query/executor.hpp"
#include "storage/storage_manager.hpp"
#include <exception>
#include <iostream>
#include <mutex>
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

int main(const int argc, char **argv) {
    std::uint16_t port = 8080;
    std::string data_dir = "./data";

    if (argc > 1) {
        const auto parsed_port = std::atoi(argv[1]);
        if (parsed_port <= 0 || parsed_port > 65535) {
            std::cerr << "usage: " << argv[0] << " [port] [data_dir]\n";
            return 1;
        }
        port = static_cast<std::uint16_t>(parsed_port);
    }
    if (argc > 2) {
        data_dir = argv[2];
    }
    if (argc > 3) {
        std::cerr << "usage: " << argv[0] << " [port] [data_dir]\n";
        return 1;
    }

    DBMS dbms;
    StorageManager storage(data_dir);
    storage.load(dbms);
    Executor exec(dbms);
    std::mutex mtx;
    crow::SimpleApp app;

    CROW_ROUTE(app, "/query").methods(crow::HTTPMethod::Post)(
        [&exec, &storage, &dbms, &mtx](const crow::request &req) {
            try {
                std::lock_guard lock(mtx);
                std::string output;
                for (const Statement &stmt: parse_sql(req.body)) {
                    if (auto r = exec.execute(stmt); !r.empty()) {
                        if (!output.empty()) output += '\n';
                        output += r;
                    }
                }
                storage.save(dbms);
                return crow::response(200, output.empty() ? "OK" : output);
            } catch (const std::exception &e) {
                return crow::response(400, e.what());
            }
        });

    std::cout << "dbms_server listening on port " << port
            << ", data dir: " << data_dir << "\n";
    app.port(port).multithreaded().run();
}
