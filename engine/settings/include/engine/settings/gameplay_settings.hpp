#pragma once

#include <string>
#include <cstdint>

namespace engine::settings {

// ============================================================================
// Difficulty Level
// ============================================================================

enum class Difficulty : uint8_t {
    Story,          // Very easy, focus on narrative
    Easy,
    Normal,
    Hard,
    VeryHard,
    Custom
};

// ============================================================================
// Colorblind Mode
// ============================================================================

enum class ColorblindMode : uint8_t {
    None,
    Protanopia,     // Red-green (red weak)
    Deuteranopia,   // Red-green (green weak)
    Tritanopia,     // Blue-yellow
    Achromatopsia   // Monochromatic
};

// ============================================================================
// Subtitle Size
// ============================================================================

enum class SubtitleSize : uint8_t {
    Small,
    Medium,
    Large,
    ExtraLarge
};

// ============================================================================
// HUD Mode
// ============================================================================

enum class HUDMode : uint8_t {
    Full,           // All HUD elements
    Minimal,        // Essential elements only
    Immersive,      // HUD appears on demand
    Off             // No HUD
};

// ============================================================================
// Gameplay Settings
// ============================================================================

struct GameplaySettings {
    // ========================================================================
    // Difficulty
    // ========================================================================

    Difficulty difficulty = Difficulty::Normal;

    // Custom difficulty modifiers (when difficulty == Custom)
    float damage_taken_multiplier = 1.0f;
    float damage_dealt_multiplier = 1.0f;
    float resource_gain_multiplier = 1.0f;
    float enemy_health_multiplier = 1.0f;
    float enemy_damage_multiplier = 1.0f;
    bool one_hit_kill = false;
    bool invincibility = false;

    // ========================================================================
    // Camera
    // ========================================================================

    float camera_distance = 1.0f;   // Multiplier
    float camera_height = 1.0f;     // Offset
    float field_of_view = 75.0f;
    bool auto_center_camera = true;
    float camera_smoothing = 0.5f;
    bool camera_collision = true;

    // ========================================================================
    // Combat
    // ========================================================================

    bool auto_lock_target = true;
    bool soft_lock_targeting = true;
    float target_switch_sensitivity = 0.5f;
    bool show_damage_numbers = true;
    bool show_enemy_health = true;

    // ========================================================================
    // Accessibility - Visual
    // ========================================================================

    ColorblindMode colorblind_mode = ColorblindMode::None;
    float colorblind_intensity = 1.0f;
    bool high_contrast_mode = false;
    float ui_scale = 1.0f;
    bool reduce_motion = false;
    bool disable_camera_shake = false;
    bool disable_screen_effects = false;  // Flashes, blood splatter, etc.

    // ========================================================================
    // Accessibility - Audio
    // ========================================================================

    bool subtitles_enabled = true;
    SubtitleSize subtitle_size = SubtitleSize::Medium;
    bool subtitle_background = true;
    float subtitle_background_opacity = 0.7f;
    bool speaker_names = true;
    bool audio_descriptions = false;
    bool closed_captions = false;   // Environmental audio descriptions

    // ========================================================================
    // Accessibility - Input
    // ========================================================================

    bool auto_sprint = false;
    bool toggle_crouch = true;
    bool quick_time_events_auto = false;
    float qte_timing_multiplier = 1.0f;

    // ========================================================================
    // HUD
    // ========================================================================

    HUDMode hud_mode = HUDMode::Full;
    bool show_minimap = true;
    bool show_compass = true;
    bool show_quest_markers = true;
    bool show_interaction_prompts = true;
    float hud_opacity = 1.0f;

    // ========================================================================
    // Gameplay Assists
    // ========================================================================

    bool navigation_assist = false;  // Path highlighting
    bool puzzle_hints = true;
    float puzzle_hint_delay = 60.0f; // Seconds before hint appears
    bool auto_pickup_items = false;
    bool auto_loot = false;

    // ========================================================================
    // Content
    // ========================================================================

    bool mature_content_enabled = true;
    bool gore_enabled = true;
    bool profanity_filter = false;

    // ========================================================================
    // Tutorials
    // ========================================================================

    bool tutorials_enabled = true;
    bool show_tips = true;
    bool repeat_tutorial_prompts = false;

    // ========================================================================
    // Language
    // ========================================================================

    std::string text_language = "en";
    std::string voice_language = "en";

    // ========================================================================
    // Methods
    // ========================================================================

    void apply_difficulty(Difficulty diff);
    void validate();
    bool operator==(const GameplaySettings& other) const = default;
};

// ============================================================================
// Helper Functions
// ============================================================================

std::string get_difficulty_name(Difficulty diff);
std::string get_colorblind_mode_name(ColorblindMode mode);
std::string get_subtitle_size_name(SubtitleSize size);
std::string get_hud_mode_name(HUDMode mode);

// Difficulty presets
float get_damage_taken_multiplier(Difficulty diff);
float get_damage_dealt_multiplier(Difficulty diff);
float get_enemy_health_multiplier(Difficulty diff);

} // namespace engine::settings
