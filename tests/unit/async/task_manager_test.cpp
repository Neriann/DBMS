#include "async/task_manager.hpp"
#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>

int main() {

    TaskManager tm([](const std::string& sql) {
        return "RESULT: " + sql;
    });

    // 1. submit
    auto id = tm.submit("SELECT 1");

    assert(!id.empty());

    // 2. wait worker
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto task = tm.get_task(id);

    assert(task.has_value());
    assert(task->status == TaskStatus::Done);
    assert(task->result == "RESULT: SELECT 1");

    std::cout << "TaskManager test PASSED\n";
}