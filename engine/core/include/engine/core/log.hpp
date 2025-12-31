#pragma once

#include <string>

namespace engine::core {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

// Log sink interface for custom log handlers
class ILogSink {
public:
    virtual ~ILogSink() = default;
    virtual void log(LogLevel level, const std::string& category, const std::string& message) = 0;
};

void log(LogLevel level, const char* message);
void set_log_level(LogLevel level);

// Register/unregister custom log sinks
void add_log_sink(ILogSink* sink);
void remove_log_sink(ILogSink* sink);

} // namespace engine::core
