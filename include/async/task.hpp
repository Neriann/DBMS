#pragma once

#include <string>

enum class TaskStatus {
    Pending,
    Running,
    Done,
    Error
};

struct Task {
    std::string id;

    std::string sql;

    TaskStatus status = TaskStatus::Pending;

    std::string result;

    std::string error;
};

std::string task_status_to_string(TaskStatus status);
