#include <engine/environment/sky_controller.hpp>
#include <engine/environment/time_of_day.hpp>
#include <engine/core/log.hpp>
#include <unordered_map>
#include <algorithm>

namespace engine::environment {

// SkyGradient lerp
SkyGradient SkyGradient::lerp(const SkyGradient& a, const SkyGradient& b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    SkyGradient result;
    result.zenith_color = glm::mix(a.zenith_color, b.zenith_color, t);
    result.horizon_color = glm::mix(a.horizon_color, b.horizon_color, t);
    result.ground_color = glm::mix(a.ground_color, b.ground_color, t);
    return result;
}

// SkyPreset lerp
SkyPreset SkyPreset::lerp(const SkyPreset& a, const SkyPreset& b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    SkyPreset result;
    result.name = t < 0.5f ? a.name : b.name;
    result.colors = SkyGradient::lerp(a.colors, b.colors, t);
    result.sun_size = glm::mix(a.sun_size, b.sun_size, t);
    result.sun_color = glm::mix(a.sun_color, b.sun_color, t);
    result.sun_intensity = glm::mix(a.sun_intensity, b.sun_intensity, t);
    result.sun_halo_color = glm::mix(a.sun_halo_color, b.sun_halo_color, t);
    result.sun_halo_size = glm::mix(a.sun_halo_size, b.sun_halo_size, t);
    result.moon_size = glm::mix(a.moon_size, b.moon_size, t);
    result.moon_color = glm::mix(a.moon_color, b.moon_color, t);
    result.moon_intensity = glm::mix(a.moon_intensity, b.moon_intensity, t);
    result.star_intensity = glm::mix(a.star_intensity, b.star_intensity, t);
    result.star_twinkle_speed = glm::mix(a.star_twinkle_speed, b.star_twinkle_speed, t);
    result.cloud_coverage = glm::mix(a.cloud_coverage, b.cloud_coverage, t);
    result.cloud_color = glm::mix(a.cloud_color, b.cloud_color, t);
    result.cloud_brightness = glm::mix(a.cloud_brightness, b.cloud_brightness, t);
    result.atmosphere_density = glm::mix(a.atmosphere_density, b.atmosphere_density, t);
    result.mie_scattering = glm::mix(a.mie_scattering, b.mie_scattering, t);
    result.horizon_fog = glm::mix(a.horizon_fog, b.horizon_fog, t);
    return result;
}

// Pre-built sky presets
namespace SkyPresets {

SkyPreset dawn() {
    SkyPreset p;
    p.name = "dawn";
    p.colors.zenith_color = Vec3(0.15f, 0.2f, 0.4f);
    p.colors.horizon_color = Vec3(0.95f, 0.6f, 0.3f);
    p.colors.ground_color = Vec3(0.2f, 0.15f, 0.1f);
    p.sun_size = 0.05f;
    p.sun_color = Vec3(1.0f, 0.7f, 0.4f);
    p.sun_intensity = 0.6f;
    p.sun_halo_color = Vec3(1.0f, 0.6f, 0.3f);
    p.sun_halo_size = 0.25f;
    p.moon_intensity = 0.1f;
    p.star_intensity = 0.2f;
    p.cloud_coverage = 0.2f;
    p.cloud_color = Vec3(1.0f, 0.85f, 0.7f);
    p.atmosphere_density = 1.2f;
    p.mie_scattering = 0.05f;
    return p;
}

SkyPreset morning() {
    SkyPreset p;
    p.name = "morning";
    p.colors.zenith_color = Vec3(0.2f, 0.4f, 0.7f);
    p.colors.horizon_color = Vec3(0.7f, 0.8f, 0.9f);
    p.colors.ground_color = Vec3(0.3f, 0.25f, 0.2f);
    p.sun_color = Vec3(1.0f, 0.95f, 0.85f);
    p.sun_intensity = 0.9f;
    p.star_intensity = 0.0f;
    p.cloud_coverage = 0.25f;
    return p;
}

SkyPreset noon() {
    SkyPreset p;
    p.name = "noon";
    p.colors.zenith_color = Vec3(0.15f, 0.35f, 0.75f);
    p.colors.horizon_color = Vec3(0.6f, 0.75f, 0.95f);
    p.colors.ground_color = Vec3(0.35f, 0.3f, 0.25f);
    p.sun_color = Vec3(1.0f, 0.98f, 0.95f);
    p.sun_intensity = 1.0f;
    p.sun_halo_size = 0.1f;
    p.star_intensity = 0.0f;
    p.cloud_coverage = 0.3f;
    p.atmosphere_density = 1.0f;
    return p;
}

SkyPreset afternoon() {
    SkyPreset p;
    p.name = "afternoon";
    p.colors.zenith_color = Vec3(0.2f, 0.4f, 0.7f);
    p.colors.horizon_color = Vec3(0.75f, 0.8f, 0.85f);
    p.colors.ground_color = Vec3(0.35f, 0.28f, 0.2f);
    p.sun_color = Vec3(1.0f, 0.95f, 0.85f);
    p.sun_intensity = 0.95f;
    p.star_intensity = 0.0f;
    p.cloud_coverage = 0.35f;
    return p;
}

SkyPreset dusk() {
    SkyPreset p;
    p.name = "dusk";
    p.colors.zenith_color = Vec3(0.15f, 0.15f, 0.35f);
    p.colors.horizon_color = Vec3(0.95f, 0.5f, 0.25f);
    p.colors.ground_color = Vec3(0.15f, 0.1f, 0.08f);
    p.sun_size = 0.055f;
    p.sun_color = Vec3(1.0f, 0.5f, 0.2f);
    p.sun_intensity = 0.5f;
    p.sun_halo_color = Vec3(1.0f, 0.4f, 0.2f);
    p.sun_halo_size = 0.3f;
    p.star_intensity = 0.3f;
    p.cloud_coverage = 0.25f;
    p.cloud_color = Vec3(1.0f, 0.7f, 0.5f);
    p.atmosphere_density = 1.3f;
    p.mie_scattering = 0.06f;
    return p;
}

SkyPreset evening() {
    SkyPreset p;
    p.name = "evening";
    p.colors.zenith_color = Vec3(0.05f, 0.08f, 0.2f);
    p.colors.horizon_color = Vec3(0.2f, 0.15f, 0.25f);
    p.colors.ground_color = Vec3(0.08f, 0.06f, 0.05f);
    p.sun_intensity = 0.1f;
    p.moon_intensity = 0.2f;
    p.star_intensity = 0.6f;
    p.cloud_coverage = 0.2f;
    p.cloud_color = Vec3(0.3f, 0.3f, 0.35f);
    p.cloud_brightness = 0.3f;
    return p;
}

SkyPreset night() {
    SkyPreset p;
    p.name = "night";
    p.colors.zenith_color = Vec3(0.02f, 0.03f, 0.08f);
    p.colors.horizon_color = Vec3(0.05f, 0.06f, 0.12f);
    p.colors.ground_color = Vec3(0.03f, 0.03f, 0.03f);
    p.sun_intensity = 0.0f;
    p.moon_size = 0.03f;
    p.moon_color = Vec3(0.9f, 0.9f, 1.0f);
    p.moon_intensity = 0.35f;
    p.star_intensity = 1.0f;
    p.star_twinkle_speed = 1.2f;
    p.cloud_coverage = 0.15f;
    p.cloud_color = Vec3(0.15f, 0.15f, 0.2f);
    p.cloud_brightness = 0.15f;
    return p;
}

SkyPreset overcast() {
    SkyPreset p;
    p.name = "overcast";
    p.colors.zenith_color = Vec3(0.5f, 0.52f, 0.55f);
    p.colors.horizon_color = Vec3(0.6f, 0.62f, 0.65f);
    p.colors.ground_color = Vec3(0.3f, 0.3f, 0.3f);
    p.sun_intensity = 0.3f;
    p.star_intensity = 0.0f;
    p.cloud_coverage = 0.95f;
    p.cloud_color = Vec3(0.7f, 0.7f, 0.72f);
    p.cloud_brightness = 0.6f;
    p.horizon_fog = 0.3f;
    return p;
}

SkyPreset stormy() {
    SkyPreset p;
    p.name = "stormy";
    p.colors.zenith_color = Vec3(0.2f, 0.22f, 0.25f);
    p.colors.horizon_color = Vec3(0.35f, 0.38f, 0.4f);
    p.colors.ground_color = Vec3(0.15f, 0.15f, 0.15f);
    p.sun_intensity = 0.15f;
    p.star_intensity = 0.0f;
    p.cloud_coverage = 1.0f;
    p.cloud_color = Vec3(0.4f, 0.42f, 0.45f);
    p.cloud_brightness = 0.4f;
    p.horizon_fog = 0.5f;
    return p;
}

} // namespace SkyPresets

// Implementation struct
struct SkyController::Impl {
    bool initialized = false;
    bool auto_time_presets = true;

    // Named presets
    std::unordered_map<std::string, SkyPreset> named_presets;

    // Time-based presets
    SkyPreset dawn_preset;
    SkyPreset morning_preset;
    SkyPreset noon_preset;
    SkyPreset afternoon_preset;
    SkyPreset dusk_preset;
    SkyPreset evening_preset;
    SkyPreset night_preset;

    // Current and target presets for blending
    SkyPreset current_preset;
    SkyPreset target_preset;
    float blend_progress = 1.0f;
    float blend_duration = 0.0f;

    // Overrides (-1 means no override)
    float cloud_coverage_override = -1.0f;
    float fog_density_override = -1.0f;
    float sun_intensity_override = -1.0f;

    // Cached result after overrides
    SkyPreset final_preset;

    void apply_overrides() {
        final_preset = current_preset;

        if (cloud_coverage_override >= 0.0f) {
            final_preset.cloud_coverage = cloud_coverage_override;
        }
        if (fog_density_override >= 0.0f) {
            final_preset.horizon_fog = fog_density_override;
        }
        if (sun_intensity_override >= 0.0f) {
            final_preset.sun_intensity = sun_intensity_override;
        }
    }

    const SkyPreset& get_preset_for_period(TimePeriod period) const {
        switch (period) {
            case TimePeriod::Dawn: return dawn_preset;
            case TimePeriod::Morning: return morning_preset;
            case TimePeriod::Noon: return noon_preset;
            case TimePeriod::Afternoon: return afternoon_preset;
            case TimePeriod::Dusk: return dusk_preset;
            case TimePeriod::Evening: return evening_preset;
            case TimePeriod::Night:
            case TimePeriod::Midnight:
            default: return night_preset;
        }
    }

    float get_blend_factor_for_hour(float hour) const {
        // Returns 0-1 within each period for smooth transitions
        auto get_period_bounds = [](TimePeriod period) -> std::pair<float, float> {
            switch (period) {
                case TimePeriod::Dawn: return {5.0f, 7.0f};
                case TimePeriod::Morning: return {7.0f, 12.0f};
                case TimePeriod::Noon: return {12.0f, 14.0f};
                case TimePeriod::Afternoon: return {14.0f, 17.0f};
                case TimePeriod::Dusk: return {17.0f, 19.0f};
                case TimePeriod::Evening: return {19.0f, 22.0f};
                case TimePeriod::Night: return {22.0f, 26.0f}; // 26 = 2am next day
                case TimePeriod::Midnight: return {2.0f, 5.0f};
                default: return {0.0f, 24.0f};
            }
        };

        // Determine current period and calculate blend within it
        TimePeriod period = get_time_of_day().get_current_period();
        auto [start, end] = get_period_bounds(period);

        // Handle wrap around midnight
        float adjusted_hour = hour;
        if (period == TimePeriod::Night && hour < 22.0f) {
            adjusted_hour += 24.0f;
        }

        float duration = end - start;
        return std::clamp((adjusted_hour - start) / duration, 0.0f, 1.0f);
    }
};

SkyController::SkyController() : m_impl(std::make_unique<Impl>()) {}

SkyController::~SkyController() = default;

void SkyController::initialize() {
    // Set default presets
    m_impl->dawn_preset = SkyPresets::dawn();
    m_impl->morning_preset = SkyPresets::morning();
    m_impl->noon_preset = SkyPresets::noon();
    m_impl->afternoon_preset = SkyPresets::afternoon();
    m_impl->dusk_preset = SkyPresets::dusk();
    m_impl->evening_preset = SkyPresets::evening();
    m_impl->night_preset = SkyPresets::night();

    // Register named presets
    m_impl->named_presets["dawn"] = m_impl->dawn_preset;
    m_impl->named_presets["morning"] = m_impl->morning_preset;
    m_impl->named_presets["noon"] = m_impl->noon_preset;
    m_impl->named_presets["afternoon"] = m_impl->afternoon_preset;
    m_impl->named_presets["dusk"] = m_impl->dusk_preset;
    m_impl->named_presets["evening"] = m_impl->evening_preset;
    m_impl->named_presets["night"] = m_impl->night_preset;
    m_impl->named_presets["overcast"] = SkyPresets::overcast();
    m_impl->named_presets["stormy"] = SkyPresets::stormy();

    // Initialize current preset based on time
    TimePeriod period = get_time_of_day().get_current_period();
    m_impl->current_preset = m_impl->get_preset_for_period(period);
    m_impl->target_preset = m_impl->current_preset;
    m_impl->final_preset = m_impl->current_preset;

    m_impl->initialized = true;

    core::log(core::LogLevel::Info, "[Environment] SkyController initialized");
}

void SkyController::update(double dt) {
    if (!m_impl->initialized) return;

    // Handle manual preset blending
    if (m_impl->blend_progress < 1.0f && m_impl->blend_duration > 0.0f) {
        m_impl->blend_progress += static_cast<float>(dt) / m_impl->blend_duration;
        m_impl->blend_progress = std::min(m_impl->blend_progress, 1.0f);

        m_impl->current_preset = SkyPreset::lerp(
            m_impl->current_preset, m_impl->target_preset, m_impl->blend_progress);
    }
    // Auto blend based on time of day
    else if (m_impl->auto_time_presets) {
        TimePeriod current_period = get_time_of_day().get_current_period();
        TimePeriod next_period;

        // Determine next period for blending
        switch (current_period) {
            case TimePeriod::Dawn: next_period = TimePeriod::Morning; break;
            case TimePeriod::Morning: next_period = TimePeriod::Noon; break;
            case TimePeriod::Noon: next_period = TimePeriod::Afternoon; break;
            case TimePeriod::Afternoon: next_period = TimePeriod::Dusk; break;
            case TimePeriod::Dusk: next_period = TimePeriod::Evening; break;
            case TimePeriod::Evening: next_period = TimePeriod::Night; break;
            case TimePeriod::Night: next_period = TimePeriod::Midnight; break;
            case TimePeriod::Midnight: next_period = TimePeriod::Dawn; break;
            default: next_period = current_period; break;
        }

        float blend = m_impl->get_blend_factor_for_hour(get_time_of_day().get_time());
        const SkyPreset& from = m_impl->get_preset_for_period(current_period);
        const SkyPreset& to = m_impl->get_preset_for_period(next_period);

        m_impl->current_preset = SkyPreset::lerp(from, to, blend);
    }

    // Apply overrides
    m_impl->apply_overrides();
}

void SkyController::shutdown() {
    m_impl->named_presets.clear();
    m_impl->initialized = false;
}

void SkyController::register_preset(const std::string& name, const SkyPreset& preset) {
    m_impl->named_presets[name] = preset;
}

const SkyPreset* SkyController::get_preset(const std::string& name) const {
    auto it = m_impl->named_presets.find(name);
    if (it != m_impl->named_presets.end()) {
        return &it->second;
    }
    return nullptr;
}

void SkyController::set_preset(const std::string& name, float blend_time) {
    const SkyPreset* preset = get_preset(name);
    if (preset) {
        set_preset(*preset, blend_time);
    } else {
        core::log(core::LogLevel::Warn, "[Environment] Sky preset '{}' not found", name);
    }
}

void SkyController::set_preset(const SkyPreset& preset, float blend_time) {
    if (blend_time <= 0.0f) {
        m_impl->current_preset = preset;
        m_impl->target_preset = preset;
        m_impl->blend_progress = 1.0f;
    } else {
        m_impl->target_preset = preset;
        m_impl->blend_duration = blend_time;
        m_impl->blend_progress = 0.0f;
    }
    m_impl->auto_time_presets = false;  // Disable auto when manually setting
}

void SkyController::set_dawn_preset(const SkyPreset& preset) {
    m_impl->dawn_preset = preset;
    m_impl->named_presets["dawn"] = preset;
}

void SkyController::set_morning_preset(const SkyPreset& preset) {
    m_impl->morning_preset = preset;
    m_impl->named_presets["morning"] = preset;
}

void SkyController::set_noon_preset(const SkyPreset& preset) {
    m_impl->noon_preset = preset;
    m_impl->named_presets["noon"] = preset;
}

void SkyController::set_afternoon_preset(const SkyPreset& preset) {
    m_impl->afternoon_preset = preset;
    m_impl->named_presets["afternoon"] = preset;
}

void SkyController::set_dusk_preset(const SkyPreset& preset) {
    m_impl->dusk_preset = preset;
    m_impl->named_presets["dusk"] = preset;
}

void SkyController::set_evening_preset(const SkyPreset& preset) {
    m_impl->evening_preset = preset;
    m_impl->named_presets["evening"] = preset;
}

void SkyController::set_night_preset(const SkyPreset& preset) {
    m_impl->night_preset = preset;
    m_impl->named_presets["night"] = preset;
}

void SkyController::set_auto_time_presets(bool enabled) {
    m_impl->auto_time_presets = enabled;
}

bool SkyController::get_auto_time_presets() const {
    return m_impl->auto_time_presets;
}

void SkyController::set_cloud_coverage_override(float coverage) {
    m_impl->cloud_coverage_override = coverage;
    m_impl->apply_overrides();
}

void SkyController::set_fog_density_override(float density) {
    m_impl->fog_density_override = density;
    m_impl->apply_overrides();
}

void SkyController::set_sun_intensity_override(float intensity) {
    m_impl->sun_intensity_override = intensity;
    m_impl->apply_overrides();
}

void SkyController::clear_overrides() {
    m_impl->cloud_coverage_override = -1.0f;
    m_impl->fog_density_override = -1.0f;
    m_impl->sun_intensity_override = -1.0f;
    m_impl->apply_overrides();
}

SkyGradient SkyController::get_current_gradient() const {
    return m_impl->final_preset.colors;
}

const SkyPreset& SkyController::get_current_preset() const {
    return m_impl->final_preset;
}

float SkyController::get_star_intensity() const {
    return m_impl->final_preset.star_intensity;
}

float SkyController::get_cloud_coverage() const {
    return m_impl->final_preset.cloud_coverage;
}

float SkyController::get_fog_density() const {
    return m_impl->final_preset.horizon_fog;
}

Vec3 SkyController::get_sun_direction() const {
    return get_time_of_day().get_sun_direction();
}

Vec3 SkyController::get_moon_direction() const {
    return get_time_of_day().get_moon_direction();
}

// Global instance
SkyController& get_sky_controller() {
    static SkyController instance;
    return instance;
}

} // namespace engine::environment
