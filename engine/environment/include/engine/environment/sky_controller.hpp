#pragma once

#include <engine/core/math.hpp>
#include <string>
#include <memory>

namespace engine::environment {

using namespace engine::core;

// Sky gradient colors at different parts of the sky dome
struct SkyGradient {
    Vec3 zenith_color{0.2f, 0.4f, 0.8f};      // Top of sky (directly above)
    Vec3 horizon_color{0.7f, 0.8f, 0.95f};    // At the horizon
    Vec3 ground_color{0.3f, 0.25f, 0.2f};     // Below horizon (for reflections)

    // Linear interpolate between two gradients
    static SkyGradient lerp(const SkyGradient& a, const SkyGradient& b, float t);
};

// Complete sky configuration at a point in time
struct SkyPreset {
    std::string name;
    SkyGradient colors;

    // Sun parameters
    float sun_size = 0.04f;                   // Angular size (0-1 scale)
    Vec3 sun_color{1.0f, 0.95f, 0.85f};       // Sun disc color
    float sun_intensity = 1.0f;               // Sun brightness multiplier
    Vec3 sun_halo_color{1.0f, 0.9f, 0.7f};    // Halo around sun
    float sun_halo_size = 0.15f;              // Halo size

    // Moon parameters
    float moon_size = 0.025f;                 // Angular size
    Vec3 moon_color{0.9f, 0.9f, 1.0f};        // Moon color
    float moon_intensity = 0.3f;              // Moon brightness

    // Stars
    float star_intensity = 0.0f;              // 0 = no stars (day), 1 = full stars (night)
    float star_twinkle_speed = 1.0f;          // Twinkle animation speed

    // Clouds
    float cloud_coverage = 0.3f;              // 0-1 cloud amount
    Vec3 cloud_color{1.0f, 1.0f, 1.0f};       // Cloud base color
    float cloud_brightness = 1.0f;            // Cloud light absorption

    // Atmosphere
    float atmosphere_density = 1.0f;          // Rayleigh scattering intensity
    float mie_scattering = 0.02f;             // Mie scattering (haze/glow around sun)
    float horizon_fog = 0.0f;                 // Additional fog at horizon (0-1)

    // Linear interpolate between two presets
    static SkyPreset lerp(const SkyPreset& a, const SkyPreset& b, float t);
};

// Pre-built sky presets for common conditions
namespace SkyPresets {
    SkyPreset dawn();           // Sunrise colors
    SkyPreset morning();        // Clear morning
    SkyPreset noon();           // Bright midday
    SkyPreset afternoon();      // Warm afternoon
    SkyPreset dusk();           // Sunset colors
    SkyPreset evening();        // Twilight
    SkyPreset night();          // Night sky with stars
    SkyPreset overcast();       // Cloudy/grey sky
    SkyPreset stormy();         // Dark storm clouds
}

// Sky controller - manages procedural sky rendering
class SkyController {
public:
    SkyController();
    ~SkyController();

    // Non-copyable
    SkyController(const SkyController&) = delete;
    SkyController& operator=(const SkyController&) = delete;

    // Initialize the sky system
    void initialize();

    // Update each frame (handles preset blending)
    void update(double dt);

    // Shutdown
    void shutdown();

    // Preset management
    void register_preset(const std::string& name, const SkyPreset& preset);
    const SkyPreset* get_preset(const std::string& name) const;

    // Set active preset (with optional blend time)
    void set_preset(const std::string& name, float blend_time = 0.0f);
    void set_preset(const SkyPreset& preset, float blend_time = 0.0f);

    // Time-based presets (automatically selected based on TimeOfDay)
    void set_dawn_preset(const SkyPreset& preset);
    void set_morning_preset(const SkyPreset& preset);
    void set_noon_preset(const SkyPreset& preset);
    void set_afternoon_preset(const SkyPreset& preset);
    void set_dusk_preset(const SkyPreset& preset);
    void set_evening_preset(const SkyPreset& preset);
    void set_night_preset(const SkyPreset& preset);

    // Enable/disable automatic preset selection based on TimeOfDay
    void set_auto_time_presets(bool enabled);
    bool get_auto_time_presets() const;

    // Manual overrides (for weather effects, etc.)
    void set_cloud_coverage_override(float coverage);  // -1 to disable override
    void set_fog_density_override(float density);      // -1 to disable override
    void set_sun_intensity_override(float intensity);  // -1 to disable override
    void clear_overrides();

    // Query current state (after all blending/overrides applied)
    SkyGradient get_current_gradient() const;
    const SkyPreset& get_current_preset() const;
    float get_star_intensity() const;
    float get_cloud_coverage() const;
    float get_fog_density() const;

    // Sun/moon queries (computed from TimeOfDay)
    Vec3 get_sun_direction() const;
    Vec3 get_moon_direction() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// Global SkyController instance accessor
SkyController& get_sky_controller();

} // namespace engine::environment
