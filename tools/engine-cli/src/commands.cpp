#include "commands.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <regex>

namespace engine::cli {

namespace fs = std::filesystem;
using json = nlohmann::json;

// Find the engine installation directory
fs::path find_engine_install_dir() {
    // Check environment variable first
    if (const char* env_dir = std::getenv("ENGINE_DIR")) {
        fs::path engine_dir = env_dir;
        if (fs::exists(engine_dir)) {
            return engine_dir;
        }
    }

    // Check common installation paths
    std::vector<fs::path> search_paths = {
#ifdef _WIN32
        "C:/Program Files/Engine",
        "C:/Engine",
#else
        "/usr/local/share/engine",
        "/opt/engine",
        fs::path(std::getenv("HOME") ? std::getenv("HOME") : "") / ".local/share/engine",
#endif
    };

    // Also check relative to the CLI executable
    fs::path exe_dir = fs::current_path();
    search_paths.push_back(exe_dir / "..");
    search_paths.push_back(exe_dir / "../..");

    for (const auto& path : search_paths) {
        if (fs::exists(path / "templates" / "game_template")) {
            return path;
        }
    }

    return {};
}

// Find the project root by looking for project.json
fs::path find_project_root() {
    fs::path current = fs::current_path();

    while (!current.empty() && current.has_parent_path()) {
        if (fs::exists(current / "project.json")) {
            return current;
        }
        auto parent = current.parent_path();
        if (parent == current) break;
        current = parent;
    }

    return {};
}

bool is_project_directory(const fs::path& dir) {
    return fs::exists(dir / "project.json");
}

// Replace placeholders in file content
std::string replace_placeholders(const std::string& content, const std::string& project_name) {
    std::string result = content;

    // Replace common placeholders
    result = std::regex_replace(result, std::regex("\\{\\{PROJECT_NAME\\}\\}"), project_name);
    result = std::regex_replace(result, std::regex("\\{\\{project_name\\}\\}"), project_name);
    result = std::regex_replace(result, std::regex("MyGame"), project_name);

    return result;
}

// Copy directory recursively with placeholder replacement
void copy_template(const fs::path& src, const fs::path& dst, const std::string& project_name) {
    fs::create_directories(dst);

    for (const auto& entry : fs::recursive_directory_iterator(src)) {
        fs::path relative = fs::relative(entry.path(), src);
        fs::path target = dst / relative;

        if (entry.is_directory()) {
            fs::create_directories(target);
        } else {
            // Check if this is a text file that needs placeholder replacement
            std::string ext = entry.path().extension().string();
            bool is_text = (ext == ".cpp" || ext == ".hpp" || ext == ".h" ||
                           ext == ".txt" || ext == ".cmake" || ext == ".json" ||
                           ext == ".md" || ext == ".in");

            if (is_text) {
                std::ifstream in(entry.path());
                std::string content((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
                in.close();

                content = replace_placeholders(content, project_name);

                std::ofstream out(target);
                out << content;
            } else {
                fs::copy_file(entry.path(), target, fs::copy_options::overwrite_existing);
            }
        }
    }
}

Result cmd_new(const std::string& project_name, const fs::path& target_dir) {
    // Validate project name
    if (project_name.empty()) {
        std::cerr << "Error: Project name cannot be empty\n";
        return Result::InvalidArgs;
    }

    // Check for invalid characters
    for (char c : project_name) {
        if (!std::isalnum(c) && c != '_' && c != '-') {
            std::cerr << "Error: Project name can only contain alphanumeric characters, underscores, and hyphens\n";
            return Result::InvalidArgs;
        }
    }

    fs::path project_dir = target_dir / project_name;

    // Check if directory already exists
    if (fs::exists(project_dir)) {
        std::cerr << "Error: Directory already exists: " << project_dir << "\n";
        return Result::FileError;
    }

    // Find engine installation
    fs::path engine_dir = find_engine_install_dir();
    if (engine_dir.empty()) {
        std::cerr << "Error: Could not find Engine installation.\n";
        std::cerr << "Set ENGINE_DIR environment variable or install Engine to a standard location.\n";
        return Result::FileError;
    }

    fs::path template_dir = engine_dir / "templates" / "game_template";
    if (!fs::exists(template_dir)) {
        std::cerr << "Error: Template not found at: " << template_dir << "\n";
        return Result::FileError;
    }

    std::cout << "Creating project '" << project_name << "' in " << project_dir << "\n";

    try {
        copy_template(template_dir, project_dir, project_name);
    } catch (const std::exception& e) {
        std::cerr << "Error copying template: " << e.what() << "\n";
        return Result::FileError;
    }

    std::cout << "\nProject created successfully!\n\n";
    std::cout << "Next steps:\n";
    std::cout << "  cd " << project_name << "\n";
    std::cout << "  engine build\n";
    std::cout << "  engine run\n";

    return Result::Success;
}

Result cmd_build(const std::string& config) {
    fs::path project_root = find_project_root();
    if (project_root.empty()) {
        std::cerr << "Error: Not in a project directory (no project.json found)\n";
        return Result::FileError;
    }

    std::cout << "Building project in " << project_root << " (" << config << ")\n";

    fs::path build_dir = project_root / "build" / config;

    // Configure if needed
    if (!fs::exists(build_dir / "CMakeCache.txt")) {
        std::cout << "Configuring CMake...\n";

        std::string configure_cmd = "cmake -S \"" + project_root.string() +
                                    "\" -B \"" + build_dir.string() +
                                    "\" -DCMAKE_BUILD_TYPE=" + config;

        int result = std::system(configure_cmd.c_str());
        if (result != 0) {
            std::cerr << "CMake configuration failed\n";
            return Result::BuildError;
        }
    }

    // Build
    std::cout << "Building...\n";
    std::string build_cmd = "cmake --build \"" + build_dir.string() +
                           "\" --config " + config;

    int result = std::system(build_cmd.c_str());
    if (result != 0) {
        std::cerr << "Build failed\n";
        return Result::BuildError;
    }

    std::cout << "Build successful!\n";
    return Result::Success;
}

Result cmd_run(bool hot_reload, const std::string& config) {
    fs::path project_root = find_project_root();
    if (project_root.empty()) {
        std::cerr << "Error: Not in a project directory (no project.json found)\n";
        return Result::FileError;
    }

    // Find the engine executable
    fs::path engine_dir = find_engine_install_dir();
    fs::path engine_exe;

    // Check common locations
    std::vector<fs::path> exe_paths = {
        engine_dir / "bin" / "Engine.exe",
        engine_dir / "bin" / "Engine",
        project_root / "build" / config / "bin" / "Engine.exe",
        project_root / "build" / config / "bin" / "Engine",
    };

    for (const auto& path : exe_paths) {
        if (fs::exists(path)) {
            engine_exe = path;
            break;
        }
    }

    if (engine_exe.empty()) {
        std::cerr << "Error: Could not find Engine executable\n";
        return Result::FileError;
    }

    // Find the game DLL
    fs::path game_dll = project_root / "build" / config / "bin" / "Game.dll";
    if (!fs::exists(game_dll)) {
        // Try without bin subdirectory
        game_dll = project_root / "build" / config / "Game.dll";
    }

    if (!fs::exists(game_dll)) {
        std::cerr << "Error: Game.dll not found. Run 'engine build' first.\n";
        return Result::FileError;
    }

    std::cout << "Running " << engine_exe << "\n";
    std::cout << "  Game DLL: " << game_dll << "\n";
    std::cout << "  Hot Reload: " << (hot_reload ? "enabled" : "disabled") << "\n";

    std::string run_cmd = "\"" + engine_exe.string() + "\"" +
                         " --game-dll=\"" + game_dll.string() + "\"" +
                         " --hot-reload=" + (hot_reload ? "on" : "off");

    // Change to project directory so assets are found
    fs::current_path(project_root);

    int result = std::system(run_cmd.c_str());
    if (result != 0) {
        std::cerr << "Engine exited with code: " << result << "\n";
        return Result::RuntimeError;
    }

    return Result::Success;
}

Result cmd_clean() {
    fs::path project_root = find_project_root();
    if (project_root.empty()) {
        std::cerr << "Error: Not in a project directory (no project.json found)\n";
        return Result::FileError;
    }

    fs::path build_dir = project_root / "build";

    if (!fs::exists(build_dir)) {
        std::cout << "Nothing to clean.\n";
        return Result::Success;
    }

    std::cout << "Removing " << build_dir << "\n";

    try {
        fs::remove_all(build_dir);
    } catch (const std::exception& e) {
        std::cerr << "Error cleaning: " << e.what() << "\n";
        return Result::FileError;
    }

    std::cout << "Clean complete.\n";
    return Result::Success;
}

void cmd_help() {
    std::cout << R"(Engine CLI - Game Development Tool

Usage: engine <command> [options]

Commands:
  new <name>      Create a new project from template
                    --path <dir>    Create in specific directory (default: current)

  build           Build the game DLL
                    --config <cfg>  Build configuration: Debug or Release (default: Debug)
                    -r, --release   Shorthand for --config Release

  run             Run the engine with the game
                    --hot-reload    Enable hot reload (default in Debug)
                    --no-hot-reload Disable hot reload
                    --config <cfg>  Configuration to run (default: Debug)
                    -r, --release   Shorthand for --config Release

  clean           Remove build artifacts

  help            Show this help message

Examples:
  engine new MyGame           Create new project 'MyGame' in current directory
  engine build                Build Debug configuration
  engine build --release      Build Release configuration
  engine run                  Run with hot reload enabled
  engine run --no-hot-reload  Run without hot reload

For more information, visit: https://github.com/yourusername/engine
)";
}

} // namespace engine::cli
