#include "async/task_manager.hpp"

#include <random>
#include <sstream>
#include <stdexcept>

TaskManager::TaskManager(
    ExecutorFunction executor
)
    : executor_(std::move(executor))
{
    worker_ = std::thread(
        &TaskManager::worker_loop,
        this
    );
}

TaskManager::~TaskManager() {
    running_ = false;

    // Отправляем пустую задачу для разблокировки очереди
    queue_.push(Task{});

    if (worker_.joinable()) {
        worker_.join();
    }
}

std::string TaskManager::submit(
    const std::string& sql
) {
    Task task;

    task.id = generate_task_id();
    task.sql = sql;
    task.status = TaskStatus::Pending;

    {
        std::lock_guard lock(tasks_mtx_);

        tasks_.emplace(task.id, task);  // emplace лучше []
    }

    queue_.push(task);

    return task.id;
}

std::optional<Task> TaskManager::get_task(
    const std::string& id
) {
    std::lock_guard lock(tasks_mtx_);

    auto it = tasks_.find(id);

    if (it == tasks_.end()) {
        return std::nullopt;
    }

    return it->second;
}

void TaskManager::worker_loop() {
    while (running_) {
        Task task = queue_.pop();

        // После разблокировки проверяем статус
        if (!running_) {
            break;
        }

        // Пустая задача-заглушка (не требуется, но для безопасности)
        if (task.id.empty()) {
            continue;
        }

        // Устанавливаем статус Running с блокировкой
        {
            std::lock_guard lock(tasks_mtx_);
            auto it = tasks_.find(task.id);
            if (it != tasks_.end()) {
                it->second.status = TaskStatus::Running;
            }
        }

        try {
            const auto result = executor_(task.sql);

            {
                std::lock_guard lock(tasks_mtx_);
                auto it = tasks_.find(task.id);
                if (it != tasks_.end()) {
                    it->second.status = TaskStatus::Done;
                    it->second.result = result;
                }
            }
        }
        catch (const std::exception& e) {
            {
                std::lock_guard lock(tasks_mtx_);
                auto it = tasks_.find(task.id);
                if (it != tasks_.end()) {
                    it->second.status = TaskStatus::Error;
                    it->second.result = e.what();
                }
            }
        }
    }
}

std::string TaskManager::generate_task_id() {
    static constexpr char chars[] =
        "0123456789abcdef";

    // thread_local генератор — создаётся один раз на поток
    static thread_local std::mt19937 gen(
        std::random_device{}()
    );

    std::uniform_int_distribution<>
        dist(0, 15);

    std::stringstream out;

    int groups[] = {8, 4, 4, 4, 12};

    for (int g = 0; g < 5; ++g) {
        if (g != 0) {
            out << '-';
        }

        for (int i = 0; i < groups[g]; ++i) {
            out << chars[dist(gen)];
        }
    }

    return out.str();
}