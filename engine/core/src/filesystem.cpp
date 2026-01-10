#include <engine/core/filesystem.hpp>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <regex>

namespace fs = std::filesystem;

namespace engine::core {

// ============================================================================
// Existing API
// ============================================================================

bool FileSystem::exists(const std::string& path) {
    std::error_code ec;
    return fs::exists(path, ec);
}

std::vector<uint8_t> FileSystem::read_binary(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return {};

    auto size = file.tellg();
    if (size < 0) return {};
    file.seekg(0);

    std::vector<uint8_t> data(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

std::string FileSystem::read_text(const std::string& path) {
    std::ifstream file(path);
    if (!file) return {};

    return std::string(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
}

bool FileSystem::write_binary(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return file.good();
}

bool FileSystem::write_text(const std::string& path, const std::string& text) {
    std::ofstream file(path);
    if (!file) return false;
    file << text;
    return file.good();
}

// ============================================================================
// Directory Operations
// ============================================================================

bool FileSystem::is_directory(const std::string& path) {
    std::error_code ec;
    return fs::is_directory(path, ec);
}

bool FileSystem::is_file(const std::string& path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec);
}

bool FileSystem::create_directory(const std::string& path) {
    std::error_code ec;
    return fs::create_directory(path, ec);
}

bool FileSystem::create_directories(const std::string& path) {
    std::error_code ec;
    return fs::create_directories(path, ec);
}

bool FileSystem::remove_file(const std::string& path) {
    std::error_code ec;
    return fs::remove(path, ec);
}

bool FileSystem::remove_directory(const std::string& path, bool recursive) {
    std::error_code ec;
    if (recursive) {
        return fs::remove_all(path, ec) != static_cast<std::uintmax_t>(-1);
    }
    return fs::remove(path, ec);
}

bool FileSystem::rename(const std::string& old_path, const std::string& new_path) {
    std::error_code ec;
    fs::rename(old_path, new_path, ec);
    return !ec;
}

bool FileSystem::copy_file(const std::string& src, const std::string& dst) {
    std::error_code ec;
    return fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
}

// ============================================================================
// File Information
// ============================================================================

FileInfo FileSystem::get_file_info(const std::string& path) {
    FileInfo info;
    info.path = path;

    std::error_code ec;
    fs::path p(path);

    info.name = p.filename().string();
    info.extension = p.extension().string();
    info.is_directory = fs::is_directory(p, ec);

    if (fs::exists(p, ec)) {
        info.size = fs::is_regular_file(p, ec) ? fs::file_size(p, ec) : 0;

        auto ftime = fs::last_write_time(p, ec);
        if (!ec) {
            auto sctp = std::chrono::time_point_cast<std::chrono::seconds>(
                std::chrono::file_clock::to_sys(ftime)
            );
            info.modified_time = static_cast<uint64_t>(sctp.time_since_epoch().count());
        }

        auto perms = fs::status(p, ec).permissions();
        info.is_readonly = (perms & fs::perms::owner_write) == fs::perms::none;
    }

    return info;
}

uint64_t FileSystem::get_file_size(const std::string& path) {
    std::error_code ec;
    auto size = fs::file_size(path, ec);
    return ec ? 0 : static_cast<uint64_t>(size);
}

uint64_t FileSystem::get_modified_time(const std::string& path) {
    std::error_code ec;
    auto ftime = fs::last_write_time(path, ec);
    if (ec) return 0;

    auto sctp = std::chrono::time_point_cast<std::chrono::seconds>(
        std::chrono::file_clock::to_sys(ftime)
    );
    return static_cast<uint64_t>(sctp.time_since_epoch().count());
}

// ============================================================================
// Directory Listing
// ============================================================================

std::vector<std::string> FileSystem::list_directory(const std::string& path) {
    std::vector<std::string> entries;
    std::error_code ec;

    if (!fs::is_directory(path, ec)) return entries;

    for (const auto& entry : fs::directory_iterator(path, ec)) {
        entries.push_back(entry.path().filename().string());
    }

    return entries;
}

std::vector<std::string> FileSystem::list_files(const std::string& path, const std::string& extension) {
    std::vector<std::string> files;
    std::error_code ec;

    if (!fs::is_directory(path, ec)) return files;

    for (const auto& entry : fs::directory_iterator(path, ec)) {
        if (entry.is_regular_file()) {
            if (extension.empty() || entry.path().extension() == extension) {
                files.push_back(entry.path().filename().string());
            }
        }
    }

    return files;
}

std::vector<std::string> FileSystem::list_directories(const std::string& path) {
    std::vector<std::string> dirs;
    std::error_code ec;

    if (!fs::is_directory(path, ec)) return dirs;

    for (const auto& entry : fs::directory_iterator(path, ec)) {
        if (entry.is_directory()) {
            dirs.push_back(entry.path().filename().string());
        }
    }

    return dirs;
}

std::vector<FileInfo> FileSystem::list_directory_info(const std::string& path) {
    std::vector<FileInfo> entries;
    std::error_code ec;

    if (!fs::is_directory(path, ec)) return entries;

    for (const auto& entry : fs::directory_iterator(path, ec)) {
        entries.push_back(get_file_info(entry.path().string()));
    }

    return entries;
}

// ============================================================================
// Recursive Operations
// ============================================================================

namespace {

// Convert glob pattern to regex
std::string glob_to_regex(const std::string& pattern) {
    std::string regex;
    for (char c : pattern) {
        switch (c) {
            case '*': regex += ".*"; break;
            case '?': regex += "."; break;
            case '.': regex += "\\."; break;
            case '[': regex += "["; break;
            case ']': regex += "]"; break;
            case '\\': regex += "\\\\"; break;
            default: regex += c; break;
        }
    }
    return regex;
}

} // anonymous namespace

std::vector<std::string> FileSystem::find_files(const std::string& root, const std::string& pattern) {
    std::vector<std::string> files;
    std::error_code ec;

    if (!fs::is_directory(root, ec)) return files;

    std::regex re(glob_to_regex(pattern), std::regex::icase);

    for (const auto& entry : fs::directory_iterator(root, ec)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            if (std::regex_match(filename, re)) {
                files.push_back(entry.path().string());
            }
        }
    }

    return files;
}

std::vector<std::string> FileSystem::find_files_recursive(const std::string& root, const std::string& pattern) {
    std::vector<std::string> files;
    std::error_code ec;

    if (!fs::is_directory(root, ec)) return files;

    std::regex re(glob_to_regex(pattern), std::regex::icase);

    for (const auto& entry : fs::recursive_directory_iterator(root, ec)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            if (std::regex_match(filename, re)) {
                files.push_back(entry.path().string());
            }
        }
    }

    return files;
}

// ============================================================================
// Path Utilities
// ============================================================================

std::string FileSystem::get_parent(const std::string& path) {
    return fs::path(path).parent_path().string();
}

std::string FileSystem::get_filename(const std::string& path) {
    return fs::path(path).filename().string();
}

std::string FileSystem::get_extension(const std::string& path) {
    return fs::path(path).extension().string();
}

std::string FileSystem::get_stem(const std::string& path) {
    return fs::path(path).stem().string();
}

std::string FileSystem::join_path(const std::string& a, const std::string& b) {
    return (fs::path(a) / fs::path(b)).string();
}

std::string FileSystem::normalize_path(const std::string& path) {
    std::error_code ec;
    auto normalized = fs::weakly_canonical(path, ec);
    return ec ? path : normalized.string();
}

std::string FileSystem::absolute_path(const std::string& path) {
    std::error_code ec;
    auto abs = fs::absolute(path, ec);
    return ec ? path : abs.string();
}

bool FileSystem::is_absolute(const std::string& path) {
    return fs::path(path).is_absolute();
}

std::string FileSystem::get_current_directory() {
    std::error_code ec;
    return fs::current_path(ec).string();
}

bool FileSystem::set_current_directory(const std::string& path) {
    std::error_code ec;
    fs::current_path(path, ec);
    return !ec;
}

} // namespace engine::core
