#pragma once

#include <engine/core/math.hpp>
#include <functional>
#include <string>
#include <memory>
#include <cstdint>

namespace engine::environment {

using namespace engine::core;

// Weather type enumeration
enum class WeatherType : uint8_t {
    Clear,          // Sunny, no clouds
    PartlyCloudy,   // Some clouds, mostly sunny
    Cloudy,         // Overcast but no precipitation
    Overcast,       // Heavy cloud cover
    LightRain,      // Light drizzle
    Rain,           // Moderate rain
    HeavyRain,      // Heavy downpour
    Thunderstorm,   // Rain with lightning and thunder
    LightSnow,      // Light snowfall
    Snow,           // Moderate snow
    Blizzard,       // Heavy snow with wind
    Fog,            // Light fog
    DenseFog,       // Thick fog, low visibility
    Sandstorm,      // Desert sandstorm (optional)
    Hail            // Hail precipitation
};

// Get string name for WeatherType
const char* weather_type_to_string(WeatherType type);

// Parse string to WeatherType
WeatherType weather_type_from_string(const std::string& name);

// Complete weather parameters
struct WeatherParams {
    WeatherType type = WeatherType::Clear;

    // Cloud coverage (0 = clear sky, 1 = fully overcast)
    float cloud_coverage = 0.0f;

    // Precipitation
    float precipitation_intensity = 0.0f;  // 0 = none, 1 = maximum
    bool precipitation_is_snow = false;     // True for snow types

    // Fog
    float fog_density = 0.0f;              // 0 = no fog, 1 = very thick
    float fog_height = 100.0f;             // Height above which fog fades
    Vec3 fog_color{0.7f, 0.75f, 0.8f};     // Fog tint

    // Wind
    float wind_speed = 0.0f;               // Meters per second
    Vec3 wind_direction{1.0f, 0.0f, 0.0f}; // Normalized direction
    float wind_gustiness = 0.0f;           // 0 = steady, 1 = very gusty

    // Surface effects
    float wetness = 0.0f;                  // Ground/surface wetness (0-1)
    float snow_accumulation = 0.0f;        // Snow buildup amount (0-1)

    // Thunder/lightning
    float thunder_frequency = 0.0f;        // Lightning strikes per minute
    float lightning_intensity = 1.0f;      // Flash brightness

    // Audio volumes (normalized 0-1, scaled by weather intensity)
    float rain_volume = 0.0f;
    float wind_volume = 0.0f;
    float thunder_volume = 0.0f;

    // Temperature (for potential future use / gameplay)
    float temperature = 20.0f;             // Celsius

    // Visibility distance (affected by fog/precipitation)
    float visibility = 1000.0f;            // Meters

    // Linear interpolate between two weather params
    static WeatherParams lerp(const WeatherParams& a, const WeatherParams& b, float t);
};

// Get default parameters for a weather type
WeatherParams get_weather_preset(WeatherType type);

// Weather system - manages weather state and transitions
class WeatherSystem {
public:
    WeatherSystem();
    ~WeatherSystem();

    // Non-copyable
    WeatherSystem(const WeatherSystem&) = delete;
    WeatherSystem& operator=(const WeatherSystem&) = delete;

    // Initialize the weather system
    void initialize();

    // Update each frame
    void update(double dt);

    // Shutdown
    void shutdown();

    // Weather control - change weather with transition time
    void set_weather(WeatherType type, float transition_time = 5.0f);
    void set_weather(const WeatherParams& params, float transition_time = 5.0f);

    // Get current weather state
    WeatherType get_current_weather() const;
    const WeatherParams& get_current_params() const;

    // Get target weather (during transition)
    WeatherType get_target_weather() const;
    const WeatherParams& get_target_params() const;

    // Transition progress (0 = at current, 1 = at target)
    float get_transition_progress() const;
    bool is_transitioning() const;

    // Cancel current transition (stay at current interpolated state)
    void cancel_transition();

    // Instant weather change (no transition)
    void set_weather_immediate(WeatherType type);
    void set_weather_immediate(const WeatherParams& params);

    // Quick queries
    bool is_raining() const;
    bool is_snowing() const;
    bool is_foggy() const;
    bool is_stormy() const;
    float get_wetness() const;
    float get_precipitation() const;
    float get_visibility() const;
    Vec3 get_wind_direction() const;
    float get_wind_speed() const;

    // Random weather generation
    void set_random_weather(float transition_time = 5.0f);

    // Weather sequence (cycle through weathers automatically)
    struct WeatherSequenceEntry {
        WeatherType type;
        float duration;          // How long to stay in this weather (seconds)
        float transition_time;   // Time to transition to next weather
    };
    void set_weather_sequence(const std::vector<WeatherSequenceEntry>& sequence, bool loop = true);
    void clear_weather_sequence();
    bool is_sequence_active() const;

    // Event callbacks
    using WeatherChangeCallback = std::function<void(WeatherType old_type, WeatherType new_type)>;
    using ThunderCallback = std::function<void(const Vec3& position, float intensity)>;

    // Register callback for weather changes
    uint32_t on_weather_change(WeatherChangeCallback callback);

    // Register callback for thunder/lightning strikes
    uint32_t on_thunder_strike(ThunderCallback callback);

    // Remove callback by ID
    void remove_callback(uint32_t id);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// Global WeatherSystem instance accessor
WeatherSystem& get_weather_system();

} // namespace engine::environment
