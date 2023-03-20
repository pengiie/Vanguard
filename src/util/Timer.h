#pragma once

#include <string>
#include <chrono>
#include <utility>
#include "../Logger.h"

#define TIMER_LOGGER_NAME "TIMER"
#define TIMER(name) vanguard::ScopedTimer timer##__LINE__(name)
#define FTIMER() TIMER(__FUNCTION__)

namespace vanguard {
    class Timer
    {
    public:
        Timer() {
            reset();
        }

        void reset() {
            m_Start = std::chrono::high_resolution_clock::now();
        }

        float elapsed() {
            return std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - m_Start).count();
        }

        float elapsedMillis() {
            return elapsed() * 1000.0f;
        }

    private:
        std::chrono::time_point<std::chrono::high_resolution_clock> m_Start;
    };

    class ScopedTimer
    {
    public:
        explicit ScopedTimer(std::string&& name)
                : m_name(name) {}
        ~ScopedTimer()
        {
            if(!LoggerRegistry::hasLogger(TIMER_LOGGER_NAME))
                LoggerRegistry::createLogger(TIMER_LOGGER_NAME);
            float time = m_Timer.elapsedMillis();
            LoggerRegistry::getLogger(TIMER_LOGGER_NAME).debug("{} - {}ms", m_name, time);
        }
    private:
        std::string m_name;
        Timer m_Timer;
    };
}