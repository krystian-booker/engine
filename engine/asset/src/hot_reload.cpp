#include <engine/asset/hot_reload.hpp>
#include <engine/core/log.hpp>
#include <unordered_map>
#include <filesystem>
#include <mutex>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace engine::asset {

using namespace engine::core;

namespace {

struct WatchEntry {
    std::string path;
    ReloadCallback callback;
    uint64_t last_modified = 0;
};

std::unordered_map<std::string, WatchEntry> s_watches;
std::mutex s_mutex;
bool s_initialized = false;

uint64_t get_file_time(const std::string& path) {
    try {
        auto ftime = std::filesystem::last_write_time(path);
        auto sctp = std::chrono::time_point_cast<std::chrono::seconds>(
            ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        return static_cast<uint64_t>(sctp.time_since_epoch().count());
    } catch (const std::exception& e) {
        log(LogLevel::Warn, ("Failed to get file time for " + path + ": " + e.what()).c_str());
        return 0;
    } catch (...) {
        log(LogLevel::Warn, ("Failed to get file time for " + path + ": unknown error").c_str());
        return 0;
    }
}

} // anonymous namespace

void HotReload::init() {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_initialized = true;
    log(LogLevel::Debug, "Hot reload system initialized");
}

void HotReload::shutdown() {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_watches.clear();
    s_initialized = false;
    log(LogLevel::Debug, "Hot reload system shutdown");
}

void HotReload::watch(const std::string& path, ReloadCallback callback) {
    std::lock_guard<std::mutex> lock(s_mutex);

    if (!s_initialized) {
        log(LogLevel::Warn, "Hot reload not initialized");
        return;
    }

    WatchEntry entry;
    entry.path = path;
    entry.callback = std::move(callback);
    entry.last_modified = get_file_time(path);

    s_watches[path] = std::move(entry);
}

void HotReload::unwatch(const std::string& path) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_watches.erase(path);
}

void HotReload::poll() {
    // Collect callbacks to invoke outside the lock to prevent deadlocks
    // (callbacks may call watch/unwatch which also need the mutex)
    std::vector<std::pair<std::string, ReloadCallback>> to_invoke;

    {
        std::lock_guard<std::mutex> lock(s_mutex);

        if (!s_initialized) return;

        for (auto& [path, entry] : s_watches) {
            uint64_t current_time = get_file_time(path);

            if (current_time > entry.last_modified && current_time != 0) {
                entry.last_modified = current_time;

                log(LogLevel::Info, ("File changed: " + path).c_str());

                if (entry.callback) {
                    to_invoke.emplace_back(path, entry.callback);
                }
            }
        }
    }

    // Invoke callbacks outside the lock
    for (const auto& [path, callback] : to_invoke) {
        try {
            callback(path);
        } catch (const std::exception& e) {
            log(LogLevel::Error, ("Hot reload callback failed for: " + path + " - " + e.what()).c_str());
        } catch (...) {
            log(LogLevel::Error, ("Hot reload callback failed for: " + path + " - unknown error").c_str());
        }
    }
}

} // namespace engine::asset
