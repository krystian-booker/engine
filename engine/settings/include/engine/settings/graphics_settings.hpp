#pragma once

#include <cstdint>
#include <string>

namespace engine::settings {

// ============================================================================
// Quality Preset
// ============================================================================

enum class QualityPreset : uint8_t {
    Low,
    Medium,
    High,
    Ultra,
    Custom
};

// ============================================================================
// Antialiasing Mode
// ============================================================================

enum class AntialiasingMode : uint8_t {
    None,
    FXAA,
    TAA,
    MSAA_2x,
    MSAA_4x,
    MSAA_8x
};

// ============================================================================
// Shadow Quality
// ============================================================================

enum class ShadowQuality : uint8_t {
    Off,
    Low,
    Medium,
    High,
    Ultra
};

// ============================================================================
// Texture Quality
// ============================================================================

enum class TextureQuality : uint8_t {
    Low,
    Medium,
    High,
    Ultra
};

// ============================================================================
// Graphics Settings
// ============================================================================

struct GraphicsSettings {
    // ========================================================================
    // Display
    // ========================================================================

    int resolution_width = 1920;
    int resolution_height = 1080;
    int refresh_rate = 60;
    bool fullscreen = false;
    bool borderless = false;
    bool vsync = true;
    int framerate_limit = 0;        // 0 = unlimited, -1 = match refresh rate
    float gamma = 1.0f;
    int monitor_index = 0;          // For multi-monitor setups

    // ========================================================================
    // Quality
    // ========================================================================

    QualityPreset preset = QualityPreset::High;
    float render_scale = 1.0f;      // Internal resolution multiplier
    ShadowQuality shadow_quality = ShadowQuality::High;
    TextureQuality texture_quality = TextureQuality::High;
    AntialiasingMode antialiasing = AntialiasingMode::TAA;
    int anisotropic_filtering = 8;  // 1, 2, 4, 8, 16

    // ========================================================================
    // Effects
    // ========================================================================

    bool bloom_enabled = true;
    float bloom_intensity = 1.0f;

    bool ambient_occlusion_enabled = true;
    bool screen_space_reflections = false;

    bool motion_blur_enabled = false;
    float motion_blur_strength = 0.5f;

    bool depth_of_field_enabled = true;
    float dof_focus_distance = 10.0f;
    float dof_aperture = 5.6f;

    bool chromatic_aberration = false;
    bool film_grain = false;
    float film_grain_intensity = 0.1f;

    bool vignette_enabled = true;
    float vignette_intensity = 0.3f;

    bool volumetric_lighting = true;
    bool volumetric_fog = true;

    // ========================================================================
    // Level of Detail
    // ========================================================================

    float lod_bias = 1.0f;          // Higher = more detail at distance
    float view_distance = 1.0f;     // Multiplier for draw distance
    float foliage_density = 1.0f;
    float shadow_distance = 1.0f;

    // ========================================================================
    // Advanced
    // ========================================================================

    bool async_compute = true;
    bool occlusion_culling = true;
    int max_lights = 32;
    bool realtime_reflections = true;
    int reflection_quality = 2;      // 0-3

    // ========================================================================
    // Methods
    // ========================================================================

    void apply_preset(QualityPreset preset);
    void validate();
    bool operator==(const GraphicsSettings& other) const = default;
};

// ============================================================================
// Helper Functions
// ============================================================================

std::string get_preset_name(QualityPreset preset);
std::string get_aa_mode_name(AntialiasingMode mode);
std::string get_shadow_quality_name(ShadowQuality quality);
std::string get_texture_quality_name(TextureQuality quality);

} // namespace engine::settings
