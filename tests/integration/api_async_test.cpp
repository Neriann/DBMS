#include <cstdlib>
#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <string>

static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    std::string s;
    std::getline(f, s);
    return s;
}

int main() {

    std::cout << "Submitting async query...\n";

    // 1. submit task
    int r1 = system(R"(
        curl -s -X POST localhost:8080/query -d "SELECT 1" > id.txt
    )");

    if (r1 != 0) return 1;

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::string id = read_file("id.txt");

    std::cout << "Task ID: " << id << "\n";

    // 2. check task status (may be pending)
    system(("curl -s localhost:8080/task/" + id + " > task1.txt").c_str());

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 3. final check
    system(("curl -s localhost:8080/task/" + id + " > task2.txt").c_str());

    std::cout << "Initial state:\n";
    system("cat task1.txt");

    std::cout << "\nFinal state:\n";
    system("cat task2.txt");

    std::cout << "\nAPI async test finished\n";

    return 0;
}