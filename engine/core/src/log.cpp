#include <engine/core/log.hpp>
#include <cstdio>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace engine::core {

static LogLevel s_log_level = LogLevel::Info;

void log(LogLevel level, const char* message) {
    if (level < s_log_level) return;
#ifdef _WIN32
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
#endif
    std::printf("%s\n", message);
}

void set_log_level(LogLevel level) {
    s_log_level = level;
}

} // namespace engine::core
