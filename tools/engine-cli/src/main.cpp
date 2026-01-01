#include "commands.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <cstring>

using namespace engine::cli;

void print_version() {
    std::cout << "Engine CLI v0.1.0\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cmd_help();
        return static_cast<int>(Result::InvalidArgs);
    }

    std::string command = argv[1];

    // Handle version flag
    if (command == "--version" || command == "-v") {
        print_version();
        return 0;
    }

    // Handle help
    if (command == "help" || command == "--help" || command == "-h") {
        cmd_help();
        return 0;
    }

    // Parse command
    if (command == "new") {
        if (argc < 3) {
            std::cerr << "Error: 'engine new' requires a project name\n";
            std::cerr << "Usage: engine new <project-name> [--path <dir>]\n";
            return static_cast<int>(Result::InvalidArgs);
        }

        std::string project_name = argv[2];
        std::filesystem::path target_dir = std::filesystem::current_path();

        // Parse optional --path argument
        for (int i = 3; i < argc; ++i) {
            if (std::strcmp(argv[i], "--path") == 0 && i + 1 < argc) {
                target_dir = argv[++i];
            }
        }

        return static_cast<int>(cmd_new(project_name, target_dir));
    }

    if (command == "build") {
        std::string config = "Debug";

        // Parse optional --config argument
        for (int i = 2; i < argc; ++i) {
            if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
                config = argv[++i];
            } else if (std::strcmp(argv[i], "-r") == 0 || std::strcmp(argv[i], "--release") == 0) {
                config = "Release";
            }
        }

        return static_cast<int>(cmd_build(config));
    }

    if (command == "run") {
        bool hot_reload = true;
        std::string config = "Debug";

        // Parse arguments
        for (int i = 2; i < argc; ++i) {
            if (std::strcmp(argv[i], "--hot-reload") == 0) {
                hot_reload = true;
            } else if (std::strcmp(argv[i], "--no-hot-reload") == 0) {
                hot_reload = false;
            } else if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
                config = argv[++i];
            } else if (std::strcmp(argv[i], "-r") == 0 || std::strcmp(argv[i], "--release") == 0) {
                config = "Release";
                hot_reload = false;  // Default to no hot reload in release
            }
        }

        return static_cast<int>(cmd_run(hot_reload, config));
    }

    if (command == "clean") {
        return static_cast<int>(cmd_clean());
    }

    std::cerr << "Unknown command: " << command << "\n";
    std::cerr << "Run 'engine help' for usage information.\n";
    return static_cast<int>(Result::InvalidArgs);
}
