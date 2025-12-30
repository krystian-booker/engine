#pragma once

namespace engine::core {

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

void log(LogLevel level, const char* message);
void set_log_level(LogLevel level);

} // namespace engine::core
