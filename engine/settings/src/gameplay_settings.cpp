#include <engine/settings/gameplay_settings.hpp>
#include <algorithm>

namespace engine::settings {

// ============================================================================
// GameplaySettings
// ============================================================================

void GameplaySettings::apply_difficulty(Difficulty diff) {
    difficulty = diff;

    switch (diff) {
        case Difficulty::Story:
            damage_taken_multiplier = 0.5f;
            damage_dealt_multiplier = 2.0f;
            resource_gain_multiplier = 2.0f;
            enemy_health_multiplier = 0.5f;
            enemy_damage_multiplier = 0.5f;
            break;

        case Difficulty::Easy:
            damage_taken_multiplier = 0.75f;
            damage_dealt_multiplier = 1.25f;
            resource_gain_multiplier = 1.25f;
            enemy_health_multiplier = 0.75f;
            enemy_damage_multiplier = 0.75f;
            break;

        case Difficulty::Normal:
            damage_taken_multiplier = 1.0f;
            damage_dealt_multiplier = 1.0f;
            resource_gain_multiplier = 1.0f;
            enemy_health_multiplier = 1.0f;
            enemy_damage_multiplier = 1.0f;
            break;

        case Difficulty::Hard:
            damage_taken_multiplier = 1.25f;
            damage_dealt_multiplier = 0.9f;
            resource_gain_multiplier = 0.9f;
            enemy_health_multiplier = 1.25f;
            enemy_damage_multiplier = 1.25f;
            break;

        case Difficulty::VeryHard:
            damage_taken_multiplier = 1.5f;
            damage_dealt_multiplier = 0.75f;
            resource_gain_multiplier = 0.75f;
            enemy_health_multiplier = 1.5f;
            enemy_damage_multiplier = 1.5f;
            break;

        case Difficulty::Custom:
            // Don't change values for custom
            break;
    }
}

void GameplaySettings::validate() {
    damage_taken_multiplier = std::clamp(damage_taken_multiplier, 0.1f, 10.0f);
    damage_dealt_multiplier = std::clamp(damage_dealt_multiplier, 0.1f, 10.0f);
    resource_gain_multiplier = std::clamp(resource_gain_multiplier, 0.1f, 10.0f);
    enemy_health_multiplier = std::clamp(enemy_health_multiplier, 0.1f, 10.0f);
    enemy_damage_multiplier = std::clamp(enemy_damage_multiplier, 0.1f, 10.0f);

    camera_distance = std::clamp(camera_distance, 0.5f, 2.0f);
    camera_height = std::clamp(camera_height, 0.5f, 2.0f);
    field_of_view = std::clamp(field_of_view, 60.0f, 120.0f);
    camera_smoothing = std::clamp(camera_smoothing, 0.0f, 1.0f);

    target_switch_sensitivity = std::clamp(target_switch_sensitivity, 0.0f, 1.0f);

    colorblind_intensity = std::clamp(colorblind_intensity, 0.0f, 1.0f);
    ui_scale = std::clamp(ui_scale, 0.5f, 2.0f);

    subtitle_background_opacity = std::clamp(subtitle_background_opacity, 0.0f, 1.0f);

    qte_timing_multiplier = std::clamp(qte_timing_multiplier, 0.5f, 3.0f);

    hud_opacity = std::clamp(hud_opacity, 0.0f, 1.0f);

    puzzle_hint_delay = std::clamp(puzzle_hint_delay, 10.0f, 600.0f);
}

// ============================================================================
// Helper Functions
// ============================================================================

std::string get_difficulty_name(Difficulty diff) {
    switch (diff) {
        case Difficulty::Story:    return "Story";
        case Difficulty::Easy:     return "Easy";
        case Difficulty::Normal:   return "Normal";
        case Difficulty::Hard:     return "Hard";
        case Difficulty::VeryHard: return "Very Hard";
        case Difficulty::Custom:   return "Custom";
        default:                   return "Unknown";
    }
}

std::string get_colorblind_mode_name(ColorblindMode mode) {
    switch (mode) {
        case ColorblindMode::None:          return "None";
        case ColorblindMode::Protanopia:    return "Protanopia (Red-Weak)";
        case ColorblindMode::Deuteranopia:  return "Deuteranopia (Green-Weak)";
        case ColorblindMode::Tritanopia:    return "Tritanopia (Blue-Yellow)";
        case ColorblindMode::Achromatopsia: return "Achromatopsia (Monochrome)";
        default:                            return "Unknown";
    }
}

std::string get_subtitle_size_name(SubtitleSize size) {
    switch (size) {
        case SubtitleSize::Small:      return "Small";
        case SubtitleSize::Medium:     return "Medium";
        case SubtitleSize::Large:      return "Large";
        case SubtitleSize::ExtraLarge: return "Extra Large";
        default:                       return "Unknown";
    }
}

std::string get_hud_mode_name(HUDMode mode) {
    switch (mode) {
        case HUDMode::Full:      return "Full";
        case HUDMode::Minimal:   return "Minimal";
        case HUDMode::Immersive: return "Immersive";
        case HUDMode::Off:       return "Off";
        default:                 return "Unknown";
    }
}

float get_damage_taken_multiplier(Difficulty diff) {
    switch (diff) {
        case Difficulty::Story:    return 0.5f;
        case Difficulty::Easy:     return 0.75f;
        case Difficulty::Normal:   return 1.0f;
        case Difficulty::Hard:     return 1.25f;
        case Difficulty::VeryHard: return 1.5f;
        default:                   return 1.0f;
    }
}

float get_damage_dealt_multiplier(Difficulty diff) {
    switch (diff) {
        case Difficulty::Story:    return 2.0f;
        case Difficulty::Easy:     return 1.25f;
        case Difficulty::Normal:   return 1.0f;
        case Difficulty::Hard:     return 0.9f;
        case Difficulty::VeryHard: return 0.75f;
        default:                   return 1.0f;
    }
}

float get_enemy_health_multiplier(Difficulty diff) {
    switch (diff) {
        case Difficulty::Story:    return 0.5f;
        case Difficulty::Easy:     return 0.75f;
        case Difficulty::Normal:   return 1.0f;
        case Difficulty::Hard:     return 1.25f;
        case Difficulty::VeryHard: return 1.5f;
        default:                   return 1.0f;
    }
}

} // namespace engine::settings
