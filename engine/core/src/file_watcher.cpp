#include <engine/core/file_watcher.hpp>
#include <engine/core/filesystem.hpp>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <algorithm>

namespace engine::core {

// ============================================================================
// Watch Entry
// ============================================================================

struct WatchEntry {
    uint32_t id = 0;
    std::string path;
    FileWatchCallback callback;
    bool recursive = false;
    bool is_file = false;

    // For polling: track known files and their modification times
    std::unordered_map<std::string, uint64_t> known_files;
};

// ============================================================================
// Pending Event (for debouncing)
// ============================================================================

struct PendingEvent {
    FileChangeEvent event;
    std::chrono::steady_clock::time_point timestamp;
    uint32_t watch_id;
};

// ============================================================================
// FileWatcher Implementation
// ============================================================================

struct FileWatcher::Impl {
    std::unordered_map<uint32_t, WatchEntry> watches;
    std::vector<PendingEvent> pending_events;
    mutable std::mutex mutex;

    uint32_t next_id = 1;
    bool paused = false;

    float debounce_time = 0.1f;     // 100ms default
    float poll_interval = 0.5f;     // 500ms default

    std::chrono::steady_clock::time_point last_poll;

    Statistics stats;

    // Scan a directory and update known files
    void scan_directory(WatchEntry& entry) {
        std::vector<std::string> current_files;

        if (entry.recursive) {
            current_files = FileSystem::find_files_recursive(entry.path, "*");
        } else {
            for (const auto& f : FileSystem::list_files(entry.path)) {
                current_files.push_back(FileSystem::join_path(entry.path, f));
            }
        }

        std::unordered_map<std::string, uint64_t> new_files;

        // Check for new and modified files
        for (const auto& file : current_files) {
            uint64_t mtime = FileSystem::get_modified_time(file);
            new_files[file] = mtime;

            auto it = entry.known_files.find(file);
            if (it == entry.known_files.end()) {
                // New file
                queue_event(entry.id, FileWatchEvent::Created, file);
            } else if (it->second != mtime) {
                // Modified file
                queue_event(entry.id, FileWatchEvent::Modified, file);
            }
        }

        // Check for deleted files
        for (const auto& [file, mtime] : entry.known_files) {
            if (new_files.find(file) == new_files.end()) {
                queue_event(entry.id, FileWatchEvent::Deleted, file);
            }
        }

        entry.known_files = std::move(new_files);
    }

    // Scan a single file for changes
    void scan_file(WatchEntry& entry) {
        bool exists = FileSystem::exists(entry.path);
        uint64_t mtime = exists ? FileSystem::get_modified_time(entry.path) : 0;

        auto it = entry.known_files.find(entry.path);

        if (it == entry.known_files.end()) {
            // First scan - just record
            if (exists) {
                entry.known_files[entry.path] = mtime;
            }
        } else if (!exists) {
            // File was deleted
            queue_event(entry.id, FileWatchEvent::Deleted, entry.path);
            entry.known_files.erase(entry.path);
        } else if (it->second != mtime) {
            // File was modified
            queue_event(entry.id, FileWatchEvent::Modified, entry.path);
            entry.known_files[entry.path] = mtime;
        }
    }

    void queue_event(uint32_t watch_id, FileWatchEvent type, const std::string& path,
                     const std::string& old_path = "") {
        auto now = std::chrono::steady_clock::now();

        FileChangeEvent event;
        event.type = type;
        event.path = path;
        event.old_path = old_path;

        auto sys_now = std::chrono::system_clock::now();
        event.timestamp = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                sys_now.time_since_epoch()
            ).count()
        );

        // Check for duplicate pending event (debouncing)
        for (auto& pending : pending_events) {
            if (pending.watch_id == watch_id &&
                pending.event.path == path &&
                pending.event.type == type) {
                // Update timestamp, don't add duplicate
                pending.timestamp = now;
                pending.event.timestamp = event.timestamp;
                stats.events_debounced++;
                return;
            }
        }

        pending_events.push_back({event, now, watch_id});
    }

    void dispatch_ready_events() {
        auto now = std::chrono::steady_clock::now();
        auto debounce_duration = std::chrono::duration<float>(debounce_time);

        std::vector<PendingEvent> still_pending;

        for (auto& pending : pending_events) {
            auto age = now - pending.timestamp;

            if (age >= debounce_duration) {
                // Ready to dispatch
                auto it = watches.find(pending.watch_id);
                if (it != watches.end() && it->second.callback) {
                    it->second.callback(pending.event);
                    stats.events_dispatched++;
                }
            } else {
                still_pending.push_back(pending);
            }
        }

        pending_events = std::move(still_pending);
    }
};

// ============================================================================
// FileWatcher Methods
// ============================================================================

FileWatcher& FileWatcher::instance() {
    static FileWatcher s_instance;
    return s_instance;
}

FileWatcher::FileWatcher() : m_impl(std::make_unique<Impl>()) {
    m_impl->last_poll = std::chrono::steady_clock::now();
}

FileWatcher::~FileWatcher() = default;

uint32_t FileWatcher::watch_directory(const std::string& path, FileWatchCallback callback,
                                       bool recursive) {
    if (!FileSystem::is_directory(path) || !callback) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(m_impl->mutex);

    WatchEntry entry;
    entry.id = m_impl->next_id++;
    entry.path = path;
    entry.callback = std::move(callback);
    entry.recursive = recursive;
    entry.is_file = false;

    // Initial scan to populate known files
    m_impl->scan_directory(entry);
    // Clear pending events from initial scan
    m_impl->pending_events.clear();

    m_impl->watches[entry.id] = std::move(entry);
    m_impl->stats.active_watches = static_cast<uint32_t>(m_impl->watches.size());

    return entry.id;
}

uint32_t FileWatcher::watch_file(const std::string& path, FileWatchCallback callback) {
    if (!callback) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(m_impl->mutex);

    WatchEntry entry;
    entry.id = m_impl->next_id++;
    entry.path = path;
    entry.callback = std::move(callback);
    entry.recursive = false;
    entry.is_file = true;

    // Initial scan
    if (FileSystem::exists(path)) {
        entry.known_files[path] = FileSystem::get_modified_time(path);
    }

    m_impl->watches[entry.id] = std::move(entry);
    m_impl->stats.active_watches = static_cast<uint32_t>(m_impl->watches.size());

    return entry.id;
}

void FileWatcher::unwatch(uint32_t watch_id) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->watches.erase(watch_id);
    m_impl->stats.active_watches = static_cast<uint32_t>(m_impl->watches.size());
}

void FileWatcher::unwatch_all() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->watches.clear();
    m_impl->pending_events.clear();
    m_impl->stats.active_watches = 0;
}

size_t FileWatcher::watch_count() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->watches.size();
}

void FileWatcher::update() {
    if (m_impl->paused) return;

    auto now = std::chrono::steady_clock::now();
    auto poll_duration = std::chrono::duration<float>(m_impl->poll_interval);

    std::lock_guard<std::mutex> lock(m_impl->mutex);

    // Check if it's time to poll
    if (now - m_impl->last_poll >= poll_duration) {
        m_impl->last_poll = now;

        // Scan all watches
        for (auto& [id, entry] : m_impl->watches) {
            if (entry.is_file) {
                m_impl->scan_file(entry);
            } else {
                m_impl->scan_directory(entry);
            }
        }
    }

    // Dispatch ready events
    m_impl->dispatch_ready_events();
}

void FileWatcher::set_debounce_time(float seconds) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->debounce_time = std::max(0.0f, seconds);
}

float FileWatcher::get_debounce_time() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->debounce_time;
}

void FileWatcher::set_poll_interval(float seconds) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->poll_interval = std::max(0.01f, seconds);
}

float FileWatcher::get_poll_interval() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->poll_interval;
}

void FileWatcher::pause() {
    m_impl->paused = true;
}

void FileWatcher::resume() {
    m_impl->paused = false;
    m_impl->last_poll = std::chrono::steady_clock::now();
}

bool FileWatcher::is_paused() const {
    return m_impl->paused;
}

FileWatcher::Statistics FileWatcher::get_statistics() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->stats;
}

void FileWatcher::reset_statistics() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->stats.events_dispatched = 0;
    m_impl->stats.events_debounced = 0;
    // Keep active_watches as-is since it's a current count
}

} // namespace engine::core
