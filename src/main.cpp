#include "core/dbms.hpp"
#include "query/scanner.hpp"
#include "parser.hpp"
#include "query/executor.hpp"
#include "storage/storage_manager.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static std::vector<Statement> parse_sql(const std::string &sql) {
    std::istringstream in(sql);
    Scanner scanner(in);
    std::vector<Statement> stmts;
    yy::Parser parser(scanner, stmts);
    parser.parse();
    return stmts;
}

static void run(const std::string &source, Executor &exec) {
    for (const Statement &stmt: parse_sql(source)) {
        try {
            if (std::string result = exec.execute(stmt); !result.empty()) std::cout << result << "\n";
        } catch (const std::exception &e) {
            std::cerr << "error: " << e.what() << "\n";
        }
    }
}

int main(int argc, char **argv) {
    DBMS dbms;
    StorageManager storage("./data");
    storage.load(dbms);

    Executor exec(dbms);

    if (argc == 1) {
        std::string line, buf;
        while (std::getline(std::cin, line)) {
            buf += line + '\n';
            if (buf.find(';') != std::string::npos) {
                try {
                    run(buf, exec);
                } catch (const std::exception &e) {
                    std::cerr << "error: " << e.what() << "\n";
                }
                buf.clear();
            }
        }
    } else {
        std::ifstream file(argv[1]);
        if (!file) {
            std::cerr << "cannot open file: " << argv[1] << "\n";
            return 1;
        }
        std::ostringstream ss;
        ss << file.rdbuf();
        try {
            run(ss.str(), exec);
        } catch (const std::exception &e) {
            std::cerr << "error: " << e.what() << "\n";
        }
    }

    storage.save(dbms);
    return 0;
}
