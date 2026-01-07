#pragma once

#include <engine/core/math.hpp>
#include <engine/environment/weather.hpp>
#include <engine/environment/time_of_day.hpp>
#include <functional>
#include <string>

namespace engine::environment {

using namespace engine::core;

// Component: Weather zone - overrides weather in a specific area
struct WeatherZone {
    // Weather override parameters
    WeatherParams override_params;

    // Blend distance at zone edges (meters)
    float blend_distance = 10.0f;

    // Zone shape (uses entity's collider or transform scale)
    enum class Shape : uint8_t {
        Box,        // Use entity's scale as box dimensions
        Sphere,     // Use entity's scale.x as radius
        Capsule     // Use scale.x as radius, scale.y as height
    };
    Shape shape = Shape::Box;

    // Priority (higher wins when overlapping zones)
    int priority = 0;

    // Time override (optional)
    bool override_time = false;
    float forced_hour = 12.0f;  // Only used if override_time is true

    // Weather transitions within zone
    float enter_transition_time = 2.0f;   // Time to blend to zone weather
    float exit_transition_time = 2.0f;    // Time to blend back to global weather

    // Is this zone currently active?
    bool enabled = true;
};

// Component: Indoor volume - marks area as sheltered from weather
struct IndoorVolume {
    // Audio effects
    float audio_dampening = 0.8f;        // How much to reduce weather audio (0-1)
    float lowpass_cutoff = 1000.0f;      // Hz, for muffled outdoor sounds

    // Visual effects
    bool block_precipitation = true;      // Stop rain/snow particles inside
    bool block_wind = true;               // Stop wind effects on vegetation
    bool reduce_ambient_light = false;    // Darken ambient lighting
    float ambient_reduction = 0.3f;       // How much to reduce ambient (0-1)

    // Zone shape
    enum class Shape : uint8_t { Box, Sphere };
    Shape shape = Shape::Box;

    // Is this volume currently active?
    bool enabled = true;
};

// Component: Time of day listener - receives callbacks for time-based events
struct TimeOfDayListener {
    // Callback for period changes (dawn, morning, noon, etc.)
    std::function<void(TimePeriod old_period, TimePeriod new_period)> on_period_change;

    // Callback for each frame update (receives current hour)
    std::function<void(float hour)> on_update;

    // Specific hour triggers (triggers once when hour is reached)
    struct HourTrigger {
        float hour;                        // Hour to trigger (0-24)
        std::function<void()> callback;
        bool triggered_today = false;      // Reset each day
    };
    std::vector<HourTrigger> hour_triggers;

    // Is this listener currently active?
    bool enabled = true;
};

// Component: Weather reactive surface - responds to weather conditions
struct WeatherReactive {
    // Wetness response
    bool affected_by_wetness = true;
    float wetness_roughness_reduction = 0.3f;  // How much to reduce PBR roughness when wet
    float wetness_darkening = 0.1f;            // How much to darken albedo when wet

    // Snow accumulation
    bool can_accumulate_snow = false;
    float snow_accumulation_rate = 0.1f;       // Units per second of snowfall
    float snow_melt_rate = 0.05f;              // Units per second when not snowing

    // Current state (set by environment system)
    float current_wetness = 0.0f;
    float current_snow = 0.0f;

    // Custom material parameter names (if different from defaults)
    std::string wetness_param = "_Wetness";
    std::string snow_param = "_SnowAmount";
};

// Component: Wind affected - responds to wind direction and speed
struct WindAffected {
    // How strongly this object responds to wind
    float wind_strength_multiplier = 1.0f;

    // Local wind offset (added to global wind)
    Vec3 local_wind_offset{0.0f};

    // Oscillation settings
    float oscillation_frequency = 1.0f;   // Base oscillation speed
    float oscillation_amplitude = 0.1f;   // Base oscillation amount

    // Mass-like resistance (higher = slower response)
    float inertia = 1.0f;

    // Current computed wind effect (set by environment system)
    Vec3 current_wind_effect{0.0f};

    // Apply to specific bones (for skeletal meshes)
    std::vector<std::string> affected_bones;

    // Is this currently active?
    bool enabled = true;
};

// Component: Lightning attractor - attracts lightning strikes
struct LightningAttractor {
    // Attraction radius (lightning will prefer to strike within this radius)
    float attraction_radius = 50.0f;

    // Attraction strength (higher = more likely to attract strikes)
    float attraction_strength = 1.0f;

    // Height bonus (taller objects naturally attract more)
    bool use_height_bonus = true;

    // Callback when lightning strikes this attractor
    std::function<void()> on_strike;

    // Minimum time between strikes (seconds)
    float strike_cooldown = 10.0f;
    float time_since_last_strike = 999.0f;
};

// Component: Environment probe - samples environment conditions at a point
struct EnvironmentProbe {
    // Cached environment state at this probe location
    float temperature = 20.0f;
    float wetness = 0.0f;
    float wind_speed = 0.0f;
    Vec3 wind_direction{0.0f};
    float light_intensity = 1.0f;
    bool is_indoor = false;

    // Update frequency (seconds between updates)
    float update_interval = 0.5f;
    float time_since_update = 0.0f;

    // Is this probe currently active?
    bool enabled = true;
};

} // namespace engine::environment
