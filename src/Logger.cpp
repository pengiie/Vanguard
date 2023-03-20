#include "Logger.h"

#include "spdlog/sinks/stdout_color_sinks.h"
#include <iostream>

#include "Application.h"

namespace vanguard {

    LoggerRegistry LoggerRegistry::s_instance{};

    Logger::Logger(const std::string& name) {
        std::vector<spdlog::sink_ptr> sinks;

        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        consoleSink->set_level(spdlog::level::trace);
        consoleSink->set_pattern("%^[%n] [%l] %v%$");
        sinks.push_back(consoleSink);

        m_logger = std::make_unique<spdlog::logger>(name, sinks.begin(), sinks.end());
        m_logger->set_level(spdlog::level::trace);
    }

    Logger::~Logger() {
        spdlog::drop(m_logger->name());
    }

    const Logger& LoggerRegistry::createLogger(const std::string& name, const std::string& displayName) {
        auto pair = instance().m_loggers.emplace(name, displayName);
        if(!pair.second)
            std::cerr << "Logger with name \'" << name << "\' already exists.";
        return pair.first->second;
    }

    const Logger& LoggerRegistry::createLogger(const std::string& name) {
        return createLogger(name, name);
    }

    const Logger& LoggerRegistry::getLogger(const std::string& name) {
        if(!hasLogger(name))
            std::cerr << "Logger with name \'" << name << "\' does not exist.";
        return instance().m_loggers.at(name);
    }

    bool LoggerRegistry::hasLogger(const std::string& name) {
        return instance().m_loggers.find(name) != instance().m_loggers.end();
    }

    LoggerRegistry& LoggerRegistry::instance() {
        return s_instance;
    }
}