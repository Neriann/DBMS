#include "async/task_manager.hpp"
#include <cassert>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <thread>
#include <chrono>

int main() {

    TaskManager tm([](const std::string& sql) {
        return "RESULT: " + sql;
    });

    // 1. submit
    auto id = tm.submit("SELECT 1");

    assert(!id.empty());
    assert(std::regex_match(
        id,
        std::regex("[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}")));

    // 2. wait worker
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto task = tm.get_task(id);

    assert(task.has_value());
    assert(task->status == TaskStatus::Done);
    assert(task->result == "RESULT: SELECT 1");

    TaskManager failed_tm([](const std::string&) -> std::string {
        throw std::runtime_error("boom");
    });

    auto failed_id = failed_tm.submit("BAD SQL");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto failed_task = failed_tm.get_task(failed_id);

    assert(failed_task.has_value());
    assert(failed_task->status == TaskStatus::Error);
    assert(failed_task->result.empty());
    assert(failed_task->error == "boom");

    std::cout << "TaskManager test PASSED\n";
}
