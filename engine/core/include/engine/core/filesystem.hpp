#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>

namespace engine::core {

// ============================================================================
// File Information
// ============================================================================

struct FileInfo {
    std::string path;
    std::string name;
    std::string extension;
    uint64_t size = 0;
    uint64_t modified_time = 0;
    bool is_directory = false;
    bool is_readonly = false;
};

// ============================================================================
// FileSystem
// ============================================================================

struct FileSystem {
    // ========================================================================
    // Existing API (unchanged)
    // ========================================================================

    static bool exists(const std::string& path);
    static std::vector<uint8_t> read_binary(const std::string& path);
    static std::string read_text(const std::string& path);
    static bool write_binary(const std::string& path, const std::vector<uint8_t>& data);
    static bool write_text(const std::string& path, const std::string& text);

    // ========================================================================
    // Directory Operations
    // ========================================================================

    // Check if path is a directory
    static bool is_directory(const std::string& path);

    // Check if path is a regular file
    static bool is_file(const std::string& path);

    // Create a single directory (parent must exist)
    static bool create_directory(const std::string& path);

    // Create directory and all parent directories
    static bool create_directories(const std::string& path);

    // Remove a file
    static bool remove_file(const std::string& path);

    // Remove a directory (must be empty unless recursive=true)
    static bool remove_directory(const std::string& path, bool recursive = false);

    // Rename/move a file or directory
    static bool rename(const std::string& old_path, const std::string& new_path);

    // Copy a file
    static bool copy_file(const std::string& src, const std::string& dst);

    // ========================================================================
    // File Information
    // ========================================================================

    // Get detailed file information
    static FileInfo get_file_info(const std::string& path);

    // Get file size in bytes
    static uint64_t get_file_size(const std::string& path);

    // Get last modified time (Unix timestamp)
    static uint64_t get_modified_time(const std::string& path);

    // ========================================================================
    // Directory Listing
    // ========================================================================

    // List all entries in a directory (files and subdirectories)
    static std::vector<std::string> list_directory(const std::string& path);

    // List only files in a directory (optionally filter by extension)
    static std::vector<std::string> list_files(const std::string& path, const std::string& extension = "");

    // List only subdirectories
    static std::vector<std::string> list_directories(const std::string& path);

    // List with full FileInfo for each entry
    static std::vector<FileInfo> list_directory_info(const std::string& path);

    // ========================================================================
    // Recursive Operations
    // ========================================================================

    // Find files matching a pattern (glob-style: *, ?)
    static std::vector<std::string> find_files(const std::string& root, const std::string& pattern);

    // Find files recursively
    static std::vector<std::string> find_files_recursive(const std::string& root, const std::string& pattern);

    // ========================================================================
    // Path Utilities
    // ========================================================================

    // Get parent directory path
    static std::string get_parent(const std::string& path);

    // Get filename from path (including extension)
    static std::string get_filename(const std::string& path);

    // Get file extension (including dot)
    static std::string get_extension(const std::string& path);

    // Get filename without extension
    static std::string get_stem(const std::string& path);

    // Join two path components
    static std::string join_path(const std::string& a, const std::string& b);

    // Normalize path (resolve . and .., fix separators)
    static std::string normalize_path(const std::string& path);

    // Convert to absolute path
    static std::string absolute_path(const std::string& path);

    // Check if path is absolute
    static bool is_absolute(const std::string& path);

    // Get current working directory
    static std::string get_current_directory();

    // Set current working directory
    static bool set_current_directory(const std::string& path);
};

} // namespace engine::core
