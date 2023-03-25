#pragma once

#include <chrono>
#include <functional>

namespace vanguard {
    typedef uint32_t TaskId;
    struct Task {
        std::function<void()> task;
        std::chrono::milliseconds delay;
        std::optional<std::chrono::milliseconds> interval;

        std::chrono::milliseconds start;
    };

    class Scheduler {
    public:
        TaskId scheduleDelayedTask(const std::function<void()>& task, std::chrono::milliseconds delay);
        TaskId scheduleRepeatingTask(const std::function<void()>& task, std::chrono::milliseconds delay, std::chrono::milliseconds interval);

        void stopTask(TaskId id);

        void update();
    private:
        std::unordered_map<TaskId, Task> m_tasks;
    };
}