#include <engine/settings/graphics_settings.hpp>
#include <algorithm>

namespace engine::settings {

// ============================================================================
// GraphicsSettings
// ============================================================================

void GraphicsSettings::apply_preset(QualityPreset p) {
    preset = p;

    switch (p) {
        case QualityPreset::Low:
            render_scale = 0.75f;
            shadow_quality = ShadowQuality::Low;
            texture_quality = TextureQuality::Low;
            antialiasing = AntialiasingMode::None;
            anisotropic_filtering = 1;
            bloom_enabled = false;
            ambient_occlusion_enabled = false;
            screen_space_reflections = false;
            motion_blur_enabled = false;
            depth_of_field_enabled = false;
            volumetric_lighting = false;
            volumetric_fog = false;
            lod_bias = 0.5f;
            view_distance = 0.5f;
            foliage_density = 0.5f;
            shadow_distance = 0.5f;
            max_lights = 8;
            realtime_reflections = false;
            reflection_quality = 0;
            break;

        case QualityPreset::Medium:
            render_scale = 1.0f;
            shadow_quality = ShadowQuality::Medium;
            texture_quality = TextureQuality::Medium;
            antialiasing = AntialiasingMode::FXAA;
            anisotropic_filtering = 4;
            bloom_enabled = true;
            bloom_intensity = 0.8f;
            ambient_occlusion_enabled = true;
            screen_space_reflections = false;
            motion_blur_enabled = false;
            depth_of_field_enabled = false;
            volumetric_lighting = false;
            volumetric_fog = true;
            lod_bias = 0.75f;
            view_distance = 0.75f;
            foliage_density = 0.75f;
            shadow_distance = 0.75f;
            max_lights = 16;
            realtime_reflections = false;
            reflection_quality = 1;
            break;

        case QualityPreset::High:
            render_scale = 1.0f;
            shadow_quality = ShadowQuality::High;
            texture_quality = TextureQuality::High;
            antialiasing = AntialiasingMode::TAA;
            anisotropic_filtering = 8;
            bloom_enabled = true;
            bloom_intensity = 1.0f;
            ambient_occlusion_enabled = true;
            screen_space_reflections = false;
            motion_blur_enabled = false;
            depth_of_field_enabled = true;
            volumetric_lighting = true;
            volumetric_fog = true;
            lod_bias = 1.0f;
            view_distance = 1.0f;
            foliage_density = 1.0f;
            shadow_distance = 1.0f;
            max_lights = 32;
            realtime_reflections = true;
            reflection_quality = 2;
            break;

        case QualityPreset::Ultra:
            render_scale = 1.0f;
            shadow_quality = ShadowQuality::Ultra;
            texture_quality = TextureQuality::Ultra;
            antialiasing = AntialiasingMode::TAA;
            anisotropic_filtering = 16;
            bloom_enabled = true;
            bloom_intensity = 1.0f;
            ambient_occlusion_enabled = true;
            screen_space_reflections = true;
            motion_blur_enabled = true;
            motion_blur_strength = 0.5f;
            depth_of_field_enabled = true;
            volumetric_lighting = true;
            volumetric_fog = true;
            lod_bias = 1.5f;
            view_distance = 1.5f;
            foliage_density = 1.0f;
            shadow_distance = 1.5f;
            max_lights = 64;
            realtime_reflections = true;
            reflection_quality = 3;
            break;

        case QualityPreset::Custom:
            // Don't change anything for custom
            break;
    }
}

void GraphicsSettings::validate() {
    resolution_width = std::max(640, resolution_width);
    resolution_height = std::max(480, resolution_height);
    refresh_rate = std::max(30, refresh_rate);
    framerate_limit = std::max(-1, framerate_limit);
    gamma = std::clamp(gamma, 0.5f, 3.0f);
    render_scale = std::clamp(render_scale, 0.25f, 2.0f);
    bloom_intensity = std::clamp(bloom_intensity, 0.0f, 2.0f);
    motion_blur_strength = std::clamp(motion_blur_strength, 0.0f, 1.0f);
    dof_focus_distance = std::max(0.1f, dof_focus_distance);
    dof_aperture = std::clamp(dof_aperture, 1.0f, 22.0f);
    film_grain_intensity = std::clamp(film_grain_intensity, 0.0f, 1.0f);
    vignette_intensity = std::clamp(vignette_intensity, 0.0f, 1.0f);
    lod_bias = std::clamp(lod_bias, 0.1f, 2.0f);
    view_distance = std::clamp(view_distance, 0.1f, 2.0f);
    foliage_density = std::clamp(foliage_density, 0.0f, 1.0f);
    shadow_distance = std::clamp(shadow_distance, 0.1f, 2.0f);
    max_lights = std::clamp(max_lights, 1, 128);
    reflection_quality = std::clamp(reflection_quality, 0, 3);
    anisotropic_filtering = std::clamp(anisotropic_filtering, 1, 16);
}

// ============================================================================
// Helper Functions
// ============================================================================

std::string get_preset_name(QualityPreset preset) {
    switch (preset) {
        case QualityPreset::Low:    return "Low";
        case QualityPreset::Medium: return "Medium";
        case QualityPreset::High:   return "High";
        case QualityPreset::Ultra:  return "Ultra";
        case QualityPreset::Custom: return "Custom";
        default:                    return "Unknown";
    }
}

std::string get_aa_mode_name(AntialiasingMode mode) {
    switch (mode) {
        case AntialiasingMode::None:    return "None";
        case AntialiasingMode::FXAA:    return "FXAA";
        case AntialiasingMode::TAA:     return "TAA";
        case AntialiasingMode::MSAA_2x: return "MSAA 2x";
        case AntialiasingMode::MSAA_4x: return "MSAA 4x";
        case AntialiasingMode::MSAA_8x: return "MSAA 8x";
        default:                        return "Unknown";
    }
}

std::string get_shadow_quality_name(ShadowQuality quality) {
    switch (quality) {
        case ShadowQuality::Off:    return "Off";
        case ShadowQuality::Low:    return "Low";
        case ShadowQuality::Medium: return "Medium";
        case ShadowQuality::High:   return "High";
        case ShadowQuality::Ultra:  return "Ultra";
        default:                    return "Unknown";
    }
}

std::string get_texture_quality_name(TextureQuality quality) {
    switch (quality) {
        case TextureQuality::Low:    return "Low";
        case TextureQuality::Medium: return "Medium";
        case TextureQuality::High:   return "High";
        case TextureQuality::Ultra:  return "Ultra";
        default:                     return "Unknown";
    }
}

} // namespace engine::settings
