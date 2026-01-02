#pragma once

#include <string>
#include <format>
#include <cstdlib>

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

// Variadic template log function for format strings
template<typename... Args>
void log(LogLevel level, std::format_string<Args...> fmt, Args&&... args) {
    log(level, std::format(fmt, std::forward<Args>(args)...).c_str());
}

} // namespace engine::core

// Runtime assertion macro - active in debug builds only
#ifdef NDEBUG
    #define ENGINE_ASSERT(condition, message) ((void)0)
#else
    #define ENGINE_ASSERT(condition, message) \
        do { \
            if (!(condition)) { \
                ::engine::core::log(::engine::core::LogLevel::Fatal, \
                    "Assertion failed: {} at {}:{}", message, __FILE__, __LINE__); \
                std::abort(); \
            } \
        } while(0)
#endif
