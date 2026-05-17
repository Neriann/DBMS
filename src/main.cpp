#include "core/dbms.hpp"
#include "query/executor.hpp"
#include "query/query_runner.hpp"
#include "storage/storage_manager.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static void run(const std::string &source, Executor &exec) {
    try {
        if (const auto result = run_sql(source, exec); !result.empty()) {
            std::cout << result << "\n";
        }
    } catch (const std::exception &e) {
        std::cerr << "error: " << e.what() << "\n";
    }
}

int main(int argc, char **argv) {
    if (argc > 2) {
        std::cerr << "usage: " << argv[0] << " [file.sql]\n";
        return 1;
    }

    DBMS dbms;
    StorageManager storage("./data");
    storage.load(dbms);

    Executor exec(dbms);

    if (argc == 1) {
        std::string line, buf;
        while (std::getline(std::cin, line)) {
            buf += line + '\n';
            if (buf.find(';') != std::string::npos) {
                run(buf, exec);
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
        run(ss.str(), exec);
    }

    storage.save(dbms);
    return 0;
}
