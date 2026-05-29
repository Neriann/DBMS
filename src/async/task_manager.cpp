#include "async/task_manager.hpp"

#include <array>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>

std::string task_status_to_string(const TaskStatus status) {
    switch (status) {
        case TaskStatus::Pending:
            return "pending";
        case TaskStatus::Running:
            return "running";
        case TaskStatus::Done:
            return "done";
        case TaskStatus::Error:
            return "error";
    }

    throw std::runtime_error("unknown task status");
}

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
                    it->second.error = e.what();
                }
            }
        }
    }
}

std::string TaskManager::generate_task_id() {
    std::array<unsigned char, 16> bytes{};

    static thread_local std::mt19937 gen(
        std::random_device{}()
    );
    std::uniform_int_distribution<int> dist(0, 255);

    for (auto& byte : bytes) {
        byte = static_cast<unsigned char>(dist(gen));
    }

    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0f) | 0x40);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3f) | 0x80);

    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::nouppercase;

    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            out << '-';
        }

        out << std::setw(2) << static_cast<int>(bytes[i]);
    }

    return out.str();
}
