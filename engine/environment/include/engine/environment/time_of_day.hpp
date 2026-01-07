#pragma once

#include <engine/core/math.hpp>
#include <functional>
#include <vector>
#include <string>
#include <memory>
#include <cstdint>

namespace engine::environment {

using namespace engine::core;

// Time periods for event callbacks
enum class TimePeriod : uint8_t {
    Dawn,       // 5:00 - 7:00
    Morning,    // 7:00 - 12:00
    Noon,       // 12:00 - 14:00
    Afternoon,  // 14:00 - 17:00
    Dusk,       // 17:00 - 19:00
    Evening,    // 19:00 - 22:00
    Night,      // 22:00 - 2:00
    Midnight    // 2:00 - 5:00
};

// Get string name for TimePeriod
const char* time_period_to_string(TimePeriod period);

// Configuration for TimeOfDay system
struct TimeOfDayConfig {
    float day_length_minutes = 24.0f;  // Real minutes per game day (24 = 1 min = 1 hour)
    float start_hour = 8.0f;           // Starting time (0-24)
    bool pause_in_menus = true;        // Pause time progression in menus
    float latitude = 45.0f;            // Latitude for sun angle calculation (degrees)
    int day_of_year = 172;             // Day of year (172 = summer solstice)
};

// Time of day management - controls the day/night cycle
class TimeOfDay {
public:
    TimeOfDay();
    ~TimeOfDay();

    // Non-copyable
    TimeOfDay(const TimeOfDay&) = delete;
    TimeOfDay& operator=(const TimeOfDay&) = delete;

    // Initialize with configuration
    void initialize(const TimeOfDayConfig& config = {});

    // Update each frame (dt in seconds)
    void update(double dt);

    // Shutdown and cleanup
    void shutdown();

    // Time control
    void set_time(float hour);             // Set time (0-24 range, wraps automatically)
    float get_time() const;                // Get current hour (0-24)
    float get_normalized_time() const;     // Get time as 0-1 range (0 = midnight, 0.5 = noon)
    void set_time_scale(float scale);      // Time multiplier (1.0 = normal, 0.0 = paused)
    float get_time_scale() const;
    void pause();                          // Pause time progression
    void resume();                         // Resume time progression
    bool is_paused() const;

    // Day counter
    int get_day() const;                   // Get current day number (starts at 0)
    void set_day(int day);                 // Set day number

    // Sun/moon calculations
    Vec3 get_sun_direction() const;        // Normalized direction TO the sun
    Vec3 get_moon_direction() const;       // Normalized direction TO the moon
    float get_sun_elevation() const;       // Sun angle above horizon (degrees, negative = below)
    float get_sun_azimuth() const;         // Sun compass direction (degrees, 0 = north)
    float get_sun_intensity() const;       // Light intensity multiplier (0-1, peaks at noon)
    bool is_sun_visible() const;           // True if sun is above horizon

    // Period queries
    TimePeriod get_current_period() const;
    bool is_daytime() const;               // True if 6:00 - 18:00
    bool is_night() const;                 // True if not daytime

    // Time string formatting
    std::string get_time_string() const;   // Format: "HH:MM"
    std::string get_time_string_12h() const; // Format: "HH:MM AM/PM"

    // Configuration access
    const TimeOfDayConfig& get_config() const;
    void set_config(const TimeOfDayConfig& config);

    // Event callbacks
    using PeriodCallback = std::function<void(TimePeriod old_period, TimePeriod new_period)>;
    using HourCallback = std::function<void(float hour)>;

    // Register callback for period changes (dawn -> morning, etc.)
    uint32_t on_period_change(PeriodCallback callback);

    // Register callback for specific hour (triggers when that hour is reached)
    uint32_t on_hour(float hour, HourCallback callback);

    // Register callback for each update (receives current hour)
    uint32_t on_update(HourCallback callback);

    // Remove callback by ID
    void remove_callback(uint32_t id);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// Global TimeOfDay instance accessor
TimeOfDay& get_time_of_day();

} // namespace engine::environment
