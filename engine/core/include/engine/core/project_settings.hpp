#pragma once

#include <engine/core/math.hpp>
#include <string>
#include <cstdint>

namespace engine::core {

struct PhysicsSettings {
    double fixed_timestep = 1.0 / 60.0;  // Default 60 Hz
    uint32_t max_substeps = 4;
    Vec3 gravity{0.0f, -9.81f, 0.0f};
};

struct RenderSettings {
    uint32_t max_draw_calls = 4096;
    bool vsync = true;
    uint32_t msaa_samples = 4;
    uint32_t shadow_map_size = 2048;
    float render_scale = 1.0f;
};

struct AudioSettings {
    uint32_t sample_rate = 48000;
    uint32_t channels = 2;
    float master_volume = 1.0f;
};

struct WindowSettings {
    uint32_t width = 1280;
    uint32_t height = 720;
    bool fullscreen = false;
    bool borderless = false;
    std::string title = "Engine";
};

struct HotReloadSettings {
    bool enabled = true;           // Master toggle (defaults to true for Debug builds)
    bool preserve_state = true;    // Serialize/deserialize world state on reload
    int poll_interval_ms = 500;    // How often to check for DLL changes
};

struct ProjectSettings {
    std::string project_name = "Untitled";
    std::string asset_directory = "assets/";
    std::string startup_scene = "";

    PhysicsSettings physics;
    RenderSettings render;
    AudioSettings audio;
    WindowSettings window;
    HotReloadSettings hot_reload;

    // Singleton access
    static ProjectSettings& get();

    // Load settings from JSON file
    bool load(const std::string& path);

    // Save settings to JSON file
    bool save(const std::string& path) const;

    // Reset to defaults
    void reset();
};

} // namespace engine::core
