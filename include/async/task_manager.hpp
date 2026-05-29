#pragma once

#include "async/task.hpp"
#include "async/task_queue.hpp"

#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <atomic>

class TaskManager {
public:
    using ExecutorFunction =
        std::function<std::string(const std::string&)>;

public:
    explicit TaskManager(ExecutorFunction executor);

    ~TaskManager();

    // Запрещаем копирование и перемещение
    TaskManager(const TaskManager&) = delete;
    TaskManager& operator=(const TaskManager&) = delete;

    TaskManager(TaskManager&&) = delete;
    TaskManager& operator=(TaskManager&&) = delete;

    std::string submit(
        const std::string& sql
    );

    std::optional<Task> get_task(
        const std::string& id
    );

private:
    void worker_loop();

    std::string generate_task_id();

private:
    TaskQueue<Task> queue_;

    std::unordered_map<
        std::string,
        Task
    > tasks_;

    std::mutex tasks_mtx_;

    ExecutorFunction executor_;

    std::thread worker_;

    std::atomic<bool> running_{true};
};