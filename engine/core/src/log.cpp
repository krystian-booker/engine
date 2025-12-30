#include <engine/core/log.hpp>
#include <bx/debug.h>

namespace engine::core {

static LogLevel s_log_level = LogLevel::Info;

void log(LogLevel level, const char* message) {
    if (level < s_log_level) return;
    bx::debugPrintf("%s\n", message);
}

void set_log_level(LogLevel level) {
    s_log_level = level;
}

} // namespace engine::core
