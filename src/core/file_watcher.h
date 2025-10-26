#pragma once
#include "core/types.h"
#include <string>
#include <unordered_map>
#include <functional>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

enum class FileAction {
    Added,
    Modified,
    Deleted
};

using FileChangeCallback = std::function<void(const std::string& filepath, FileAction action)>;

class FileWatcher {
public:
    FileWatcher() = default;
    ~FileWatcher() = default;

    // Start watching a directory
    void WatchDirectory(const std::string& directory, bool recursive = true);

    // Stop watching a directory
    void UnwatchDirectory(const std::string& directory);

    // Register callback for specific file extension
    // Example: RegisterCallback(".glsl", [](path, action) { ReloadShader(path); });
    void RegisterCallback(const std::string& extension, FileChangeCallback callback);

    // Check for file changes (call each frame)
    void Update();

private:
    struct FileEntry {
        fs::file_time_type lastWriteTime;
        bool exists;
    };

    std::vector<std::string> m_WatchedDirectories;
    std::unordered_map<std::string, FileEntry> m_FileCache;
    std::unordered_map<std::string, std::vector<FileChangeCallback>> m_Callbacks;

    bool m_Recursive = true;

    // Helper functions
    void ScanDirectory(const std::string& directory);
    void CheckFile(const std::string& filepath);
};
