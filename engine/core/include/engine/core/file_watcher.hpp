#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <cstdint>

namespace engine::core {

// ============================================================================
// File Watch Event Types
// ============================================================================

enum class FileWatchEvent : uint8_t {
    Created,    // File or directory was created
    Modified,   // File was modified
    Deleted,    // File or directory was deleted
    Renamed     // File or directory was renamed
};

// ============================================================================
// File Change Event
// ============================================================================

struct FileChangeEvent {
    FileWatchEvent type;
    std::string path;           // Current path (or new path for renamed)
    std::string old_path;       // Previous path (for renamed events)
    uint64_t timestamp;         // Time of event (Unix timestamp)
};

// ============================================================================
// Callback Types
// ============================================================================

using FileWatchCallback = std::function<void(const FileChangeEvent&)>;

// ============================================================================
// FileWatcher
// ============================================================================

class FileWatcher {
public:
    // Singleton access
    static FileWatcher& instance();

    // Delete copy/move
    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;

    // ========================================================================
    // Watch Management
    // ========================================================================

    // Watch a directory for changes
    // Returns watch ID (0 on failure)
    uint32_t watch_directory(const std::string& path, FileWatchCallback callback,
                              bool recursive = false);

    // Watch a single file for changes
    // Returns watch ID (0 on failure)
    uint32_t watch_file(const std::string& path, FileWatchCallback callback);

    // Stop watching a path
    void unwatch(uint32_t watch_id);

    // Stop all watches
    void unwatch_all();

    // Get number of active watches
    size_t watch_count() const;

    // ========================================================================
    // Update
    // ========================================================================

    // Check for file changes and dispatch callbacks
    // Call once per frame or periodically
    void update();

    // ========================================================================
    // Configuration
    // ========================================================================

    // Set debounce time (avoid multiple events for rapid changes)
    void set_debounce_time(float seconds);
    float get_debounce_time() const;

    // Set poll interval for fallback polling mode
    void set_poll_interval(float seconds);
    float get_poll_interval() const;

    // ========================================================================
    // Pause/Resume
    // ========================================================================

    // Pause watching (useful during builds or batch operations)
    void pause();

    // Resume watching
    void resume();

    // Check if paused
    bool is_paused() const;

    // ========================================================================
    // Statistics
    // ========================================================================

    struct Statistics {
        uint64_t events_dispatched = 0;
        uint64_t events_debounced = 0;
        uint32_t active_watches = 0;
    };

    Statistics get_statistics() const;
    void reset_statistics();

private:
    FileWatcher();
    ~FileWatcher();

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// ============================================================================
// Global Access
// ============================================================================

inline FileWatcher& file_watcher() { return FileWatcher::instance(); }

} // namespace engine::core
