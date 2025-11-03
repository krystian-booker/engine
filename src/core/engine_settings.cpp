#include "engine_settings.h"
#include "platform/platform.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;
using json = nlohmann::json;

std::string EngineSettings::GetEngineConfigDirectory() {
    return Platform::GetAppDataDirectory("CustomEngine");
}

EngineSettings EngineSettings::Load() {
    EngineSettings settings;

    try {
        std::string configDir = GetEngineConfigDirectory();
        if (configDir.empty()) {
            std::cerr << "Failed to get engine config directory" << std::endl;
            return settings;
        }

        std::string configPath = configDir + "/engine_settings.json";

        if (!fs::exists(configPath)) {
            std::cout << "Engine settings file not found at " << configPath << ", using defaults" << std::endl;
            return settings;
        }

        std::ifstream file(configPath);
        if (!file.is_open()) {
            std::cerr << "Failed to open engine settings file" << std::endl;
            return settings;
        }

        json j;
        file >> j;
        file.close();

        // Parse settings
        if (j.contains("projectSelection")) {
            auto& ps = j["projectSelection"];
            settings.skipProjectPicker = ps.value("skipProjectPicker", false);
            settings.defaultProjectPath = ps.value("defaultProjectPath", "");
        }

        if (j.contains("editorPreferences")) {
            auto& ep = j["editorPreferences"];
            settings.showDebugUI = ep.value("showDebugUI", true);
            settings.showFPS = ep.value("showFPS", true);
            settings.editorCameraMoveSpeed = ep.value("cameraMoveSpeed", 5.0f);
            settings.editorCameraRotateSpeed = ep.value("cameraRotateSpeed", 0.1f);
        }

        return settings;
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to load engine settings: " << e.what() << std::endl;
        return settings;
    }
}

bool EngineSettings::Save() const {
    try {
        std::string configDir = GetEngineConfigDirectory();
        if (configDir.empty()) {
            std::cerr << "Failed to get engine config directory" << std::endl;
            return false;
        }

        // Ensure config directory exists (GetAppDataDirectory should create it, but double-check)
        fs::create_directories(configDir);

        // Create JSON object
        json j;

        j["projectSelection"] = {
            {"skipProjectPicker", skipProjectPicker},
            {"defaultProjectPath", defaultProjectPath}
        };

        j["editorPreferences"] = {
            {"showDebugUI", showDebugUI},
            {"showFPS", showFPS},
            {"cameraMoveSpeed", editorCameraMoveSpeed},
            {"cameraRotateSpeed", editorCameraRotateSpeed}
        };

        // Write to file
        std::string configPath = configDir + "/engine_settings.json";
        std::ofstream file(configPath);
        if (!file.is_open()) {
            std::cerr << "Failed to open engine settings file for writing at " << configPath << std::endl;
            return false;
        }

        file << j.dump(4);  // Pretty print with 4 spaces
        file.close();

        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to save engine settings: " << e.what() << std::endl;
        return false;
    }
}
