#include "file_watcher.h"
#include <iostream>
#include <algorithm>

void FileWatcher::WatchDirectory(const std::string& directory, bool recursive) {
    m_Recursive = recursive;

    if (!fs::exists(directory) || !fs::is_directory(directory)) {
        std::cerr << "Directory not found: " << directory << std::endl;
        return;
    }

    m_WatchedDirectories.push_back(directory);
    ScanDirectory(directory);

    std::cout << "Watching directory: " << directory
              << (recursive ? " (recursive)" : "") << std::endl;
}

void FileWatcher::UnwatchDirectory(const std::string& directory) {
    auto it = std::find(m_WatchedDirectories.begin(), m_WatchedDirectories.end(), directory);
    if (it != m_WatchedDirectories.end()) {
        m_WatchedDirectories.erase(it);
        std::cout << "Stopped watching: " << directory << std::endl;
    }
}

void FileWatcher::RegisterCallback(const std::string& extension, FileChangeCallback callback) {
    m_Callbacks[extension].push_back(callback);
    std::cout << "Registered file watcher callback for: " << extension << std::endl;
}

void FileWatcher::Update() {
    // Scan all watched directories
    for (const auto& directory : m_WatchedDirectories) {
        ScanDirectory(directory);
    }

    // Check for deleted files
    std::vector<std::string> deletedFiles;
    for (auto& [filepath, entry] : m_FileCache) {
        if (entry.exists && !fs::exists(filepath)) {
            entry.exists = false;
            deletedFiles.push_back(filepath);
        }
    }

    // Notify callbacks for deleted files
    for (const auto& filepath : deletedFiles) {
        std::string ext = fs::path(filepath).extension().string();
        auto it = m_Callbacks.find(ext);
        if (it != m_Callbacks.end()) {
            for (auto& callback : it->second) {
                callback(filepath, FileAction::Deleted);
            }
        }
    }
}

void FileWatcher::ScanDirectory(const std::string& directory) {
    try {
        if (m_Recursive) {
            for (const auto& entry : fs::recursive_directory_iterator(directory)) {
                if (entry.is_regular_file()) {
                    CheckFile(entry.path().string());
                }
            }
        } else {
            for (const auto& entry : fs::directory_iterator(directory)) {
                if (entry.is_regular_file()) {
                    CheckFile(entry.path().string());
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "FileWatcher error: " << e.what() << std::endl;
    }
}

void FileWatcher::CheckFile(const std::string& filepath) {
    auto lastWriteTime = fs::last_write_time(filepath);

    auto it = m_FileCache.find(filepath);
    if (it == m_FileCache.end()) {
        // New file detected
        m_FileCache[filepath] = {lastWriteTime, true};

        std::string ext = fs::path(filepath).extension().string();
        auto callbackIt = m_Callbacks.find(ext);
        if (callbackIt != m_Callbacks.end()) {
            std::cout << "File added: " << filepath << std::endl;
            for (auto& callback : callbackIt->second) {
                callback(filepath, FileAction::Added);
            }
        }
    } else if (it->second.lastWriteTime != lastWriteTime) {
        // File modified
        it->second.lastWriteTime = lastWriteTime;
        it->second.exists = true;

        std::string ext = fs::path(filepath).extension().string();
        auto callbackIt = m_Callbacks.find(ext);
        if (callbackIt != m_Callbacks.end()) {
            std::cout << "File modified: " << filepath << std::endl;
            for (auto& callback : callbackIt->second) {
                callback(filepath, FileAction::Modified);
            }
        }
    }
}
