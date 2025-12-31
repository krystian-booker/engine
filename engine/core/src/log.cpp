#include <engine/core/log.hpp>
#include <cstdio>
#include <vector>
#include <mutex>
#include <algorithm>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace engine::core {

static LogLevel s_log_level = LogLevel::Info;
static std::vector<ILogSink*> s_log_sinks;
static std::mutex s_sink_mutex;

void log(LogLevel level, const char* message) {
    if (level < s_log_level) return;
#ifdef _WIN32
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
#endif
    std::printf("%s\n", message);

    // Forward to registered sinks
    std::lock_guard<std::mutex> lock(s_sink_mutex);
    for (auto* sink : s_log_sinks) {
        if (sink) {
            sink->log(level, "", message);
        }
    }
}

void set_log_level(LogLevel level) {
    s_log_level = level;
}

void add_log_sink(ILogSink* sink) {
    if (!sink) return;
    std::lock_guard<std::mutex> lock(s_sink_mutex);
    s_log_sinks.push_back(sink);
}

void remove_log_sink(ILogSink* sink) {
    if (!sink) return;
    std::lock_guard<std::mutex> lock(s_sink_mutex);
    s_log_sinks.erase(
        std::remove(s_log_sinks.begin(), s_log_sinks.end(), sink),
        s_log_sinks.end()
    );
}

} // namespace engine::core
