#pragma once

#include <string>
#include <filesystem>

namespace engine::cli {

// Command result codes
enum class Result {
    Success = 0,
    InvalidArgs = 1,
    FileError = 2,
    BuildError = 3,
    RuntimeError = 4
};

// engine new <project-name> [--path <dir>]
// Creates a new project from the template
Result cmd_new(const std::string& project_name, const std::filesystem::path& target_dir);

// engine build [--config Debug|Release]
// Builds the game DLL in the current project directory
Result cmd_build(const std::string& config);

// engine run [--hot-reload] [--config Debug|Release]
// Runs the engine with the current project's game DLL
Result cmd_run(bool hot_reload, const std::string& config);

// engine clean
// Cleans build artifacts
Result cmd_clean();

// engine help
// Prints help information
void cmd_help();

// Utility functions
std::filesystem::path find_engine_install_dir();
std::filesystem::path find_project_root();
bool is_project_directory(const std::filesystem::path& dir);

} // namespace engine::cli
