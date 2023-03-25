#include "Scheduler.h"

namespace vanguard {
    TaskId Scheduler::scheduleDelayedTask(const std::function<void()>& task, std::chrono::milliseconds delay) {
        uint32_t id = m_tasks.size();
        m_tasks.emplace(id, Task{
            .task = task,
            .delay = delay,
            .start = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
        });
        return id;
    }

    TaskId Scheduler::scheduleRepeatingTask(const std::function<void()>& task, std::chrono::milliseconds delay, std::chrono::milliseconds interval) {
        uint32_t id = m_tasks.size();
        m_tasks.emplace(id, Task{
            .task = task,
            .delay = delay,
            .interval = interval,
            .start = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
        });
        return id;
    }

    void Scheduler::stopTask(TaskId id) {
        m_tasks.erase(id);
    }

    void Scheduler::update() {
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
        for(auto& [id, task] : m_tasks) {
            if(now - task.start >= task.delay) {
                task.task();
                if(task.interval.has_value()) {
                    task.start = now;
                    task.delay = task.interval.value();
                } else {
                    m_tasks.erase(id);
                }
            }
        }
    }
}