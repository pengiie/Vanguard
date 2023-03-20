#pragma once

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#include "Config.h"
#include "spdlog/spdlog.h"
#include "util/Singleton.h"
#include <string>
#include <memory>
#include <unordered_map>

#define LOG_TYPE(name, spdlogLevel) inline void name(const std::string& message) const { log(spdlogLevel, message); } \
                                    template<typename... Args>                                                  \
                                    inline void name(const fmt::format_string<Args...>& formatString, Args&&... args) const { log(spdlogLevel, formatString, std::forward<Args>(args)...); }
namespace vanguard {
    class Logger {
    public:
        explicit Logger(const std::string& name);
        ~Logger();

        void log(spdlog::level::level_enum level, const std::string& message) const {
            m_logger->log(level, message);
        }

        template<typename... Args>
        void log(spdlog::level::level_enum level, const fmt::format_string<Args...>& formatString, Args&&... args) const {
            m_logger->log(level, formatString, std::forward<Args>(args)...);
        }

        LOG_TYPE(trace, spdlog::level::trace);
        LOG_TYPE(debug, spdlog::level::debug);
        LOG_TYPE(info, spdlog::level::info);
        LOG_TYPE(warn, spdlog::level::warn);
        LOG_TYPE(error, spdlog::level::err);

    private:
        std::unique_ptr<spdlog::logger> m_logger;
    };

    class LoggerRegistry : public Singleton {
    public:
        static const Logger& createLogger(const std::string& name);
        static const Logger& createLogger(const std::string& name, const std::string& displayName);
        static const Logger& getLogger(const std::string& name);
        static bool hasLogger(const std::string& name);
    private:
        static LoggerRegistry& instance();
    private:
        std::unordered_map<std::string, Logger> m_loggers;

        static LoggerRegistry s_instance;
    };

    #define LOG(level, ...) vanguard::LoggerRegistry::getLogger(APPLICATION_NAME).log(level, __VA_ARGS__)
    #define TRACE(...) LOG(spdlog::level::trace, __VA_ARGS__)
    #define DEBUG(...) LOG(spdlog::level::debug, __VA_ARGS__)
    #define INFO(...) LOG(spdlog::level::info, __VA_ARGS__)
    #define WARN(...) LOG(spdlog::level::warn, __VA_ARGS__)
    #define ERROR(...) LOG(spdlog::level::err, __VA_ARGS__)

}