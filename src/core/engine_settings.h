#pragma once

#include "core/types.h"
#include <string>

/**
 * EngineSettings
 *
 * Global engine-wide settings that persist across sessions.
 * Includes default project path and whether to skip the project picker.
 */
struct EngineSettings {
    // Project selection behavior
    bool skipProjectPicker = false;
    std::string defaultProjectPath;

    // Editor preferences
    bool showDebugUI = true;
    bool showFPS = true;
    f32 editorCameraMoveSpeed = 5.0f;
    f32 editorCameraRotateSpeed = 0.1f;

    // Load from config/engine_settings.json
    static EngineSettings Load();

    // Save to config/engine_settings.json
    bool Save() const;
};
