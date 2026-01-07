#include <engine/environment/time_of_day.hpp>
#include <engine/core/log.hpp>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace engine::environment {

// Convert TimePeriod to string
const char* time_period_to_string(TimePeriod period) {
    switch (period) {
        case TimePeriod::Dawn: return "Dawn";
        case TimePeriod::Morning: return "Morning";
        case TimePeriod::Noon: return "Noon";
        case TimePeriod::Afternoon: return "Afternoon";
        case TimePeriod::Dusk: return "Dusk";
        case TimePeriod::Evening: return "Evening";
        case TimePeriod::Night: return "Night";
        case TimePeriod::Midnight: return "Midnight";
        default: return "Unknown";
    }
}

// Implementation struct
struct TimeOfDay::Impl {
    TimeOfDayConfig config;
    float current_hour = 8.0f;
    float time_scale = 1.0f;
    bool paused = false;
    int current_day = 0;
    TimePeriod current_period = TimePeriod::Morning;

    // Derived values
    float seconds_per_hour = 0.0f;

    // Callback storage
    struct PeriodCallbackEntry {
        uint32_t id;
        TimeOfDay::PeriodCallback callback;
    };
    struct HourCallbackEntry {
        uint32_t id;
        float hour;
        TimeOfDay::HourCallback callback;
        bool triggered_today = false;
    };
    struct UpdateCallbackEntry {
        uint32_t id;
        TimeOfDay::HourCallback callback;
    };

    std::vector<PeriodCallbackEntry> period_callbacks;
    std::vector<HourCallbackEntry> hour_callbacks;
    std::vector<UpdateCallbackEntry> update_callbacks;
    uint32_t next_callback_id = 1;

    // Cached sun/moon calculations
    mutable Vec3 cached_sun_direction{0.0f, 1.0f, 0.0f};
    mutable Vec3 cached_moon_direction{0.0f, -1.0f, 0.0f};
    mutable float cached_sun_elevation = 45.0f;
    mutable float cached_sun_azimuth = 180.0f;
    mutable float cached_hour_for_sun = -1.0f;

    void update_sun_position() const {
        if (std::abs(cached_hour_for_sun - current_hour) < 0.001f) {
            return;  // Already computed for this hour
        }
        cached_hour_for_sun = current_hour;

        // Simplified sun position calculation
        // In a full implementation, you'd use proper astronomical calculations
        // based on latitude, day of year, etc.

        // Convert hour to angle (0h = -180, 12h = 0, 24h = 180)
        float hour_angle = (current_hour - 12.0f) * 15.0f;  // 15 degrees per hour

        // Calculate sun elevation based on time
        // Peak at noon, below horizon at night
        // Simple sinusoidal approximation
        float day_fraction = current_hour / 24.0f;
        float elevation_angle = 90.0f * std::sin(day_fraction * 3.14159f * 2.0f - 3.14159f / 2.0f);

        // Adjust for latitude (simplified)
        float latitude_rad = config.latitude * 3.14159f / 180.0f;
        float max_elevation = 90.0f - std::abs(config.latitude);

        // Apply day of year variation (simplified)
        // Summer solstice (day 172) = max elevation, winter solstice (day 355) = min
        float day_variation = std::cos((config.day_of_year - 172) * 3.14159f / 182.5f);
        elevation_angle *= (0.5f + 0.5f * day_variation);

        // Clamp elevation
        cached_sun_elevation = std::clamp(elevation_angle, -90.0f, max_elevation);

        // Calculate azimuth (simplified - east in morning, west in evening)
        cached_sun_azimuth = 180.0f + hour_angle;

        // Convert to direction vector
        float elev_rad = cached_sun_elevation * 3.14159f / 180.0f;
        float azim_rad = cached_sun_azimuth * 3.14159f / 180.0f;

        cached_sun_direction.x = std::cos(elev_rad) * std::sin(azim_rad);
        cached_sun_direction.y = std::sin(elev_rad);
        cached_sun_direction.z = std::cos(elev_rad) * std::cos(azim_rad);
        cached_sun_direction = glm::normalize(cached_sun_direction);

        // Moon is roughly opposite to sun (simplified)
        cached_moon_direction = -cached_sun_direction;
        // Add slight offset for realism
        cached_moon_direction.x += 0.1f;
        cached_moon_direction = glm::normalize(cached_moon_direction);
    }

    TimePeriod calculate_period(float hour) const {
        if (hour >= 5.0f && hour < 7.0f) return TimePeriod::Dawn;
        if (hour >= 7.0f && hour < 12.0f) return TimePeriod::Morning;
        if (hour >= 12.0f && hour < 14.0f) return TimePeriod::Noon;
        if (hour >= 14.0f && hour < 17.0f) return TimePeriod::Afternoon;
        if (hour >= 17.0f && hour < 19.0f) return TimePeriod::Dusk;
        if (hour >= 19.0f && hour < 22.0f) return TimePeriod::Evening;
        if (hour >= 22.0f || hour < 2.0f) return TimePeriod::Night;
        return TimePeriod::Midnight;  // 2:00 - 5:00
    }

    void check_hour_callbacks(float old_hour, float new_hour) {
        // Handle day wrap
        bool wrapped = new_hour < old_hour;

        for (auto& entry : hour_callbacks) {
            if (entry.triggered_today) {
                // Reset if we wrapped to a new day
                if (wrapped) {
                    entry.triggered_today = false;
                }
                continue;
            }

            // Check if we crossed this hour
            bool should_trigger = false;
            if (wrapped) {
                // Crossed midnight - check if hour is after old or before new
                should_trigger = (entry.hour >= old_hour) || (entry.hour < new_hour);
            } else {
                should_trigger = (entry.hour >= old_hour) && (entry.hour < new_hour);
            }

            if (should_trigger && entry.callback) {
                entry.callback(entry.hour);
                entry.triggered_today = true;
            }
        }
    }
};

TimeOfDay::TimeOfDay() : m_impl(std::make_unique<Impl>()) {}

TimeOfDay::~TimeOfDay() = default;

void TimeOfDay::initialize(const TimeOfDayConfig& config) {
    m_impl->config = config;
    m_impl->current_hour = config.start_hour;
    m_impl->current_period = m_impl->calculate_period(m_impl->current_hour);

    // Calculate seconds per hour based on day length
    // If day_length_minutes = 24, then 1 real minute = 1 game hour
    // So 1 real second = 1/60 game hour
    // seconds_per_hour = 60 / day_length_minutes * 24 / 24 = 60 / day_length_minutes
    // Wait, let's think again:
    // day_length_minutes = real minutes for a 24 hour day
    // So game_hours_per_real_second = 24 / (day_length_minutes * 60)
    // We want seconds_per_hour (real seconds to advance 1 game hour)
    // seconds_per_hour = day_length_minutes * 60 / 24 = day_length_minutes * 2.5
    m_impl->seconds_per_hour = config.day_length_minutes * 60.0f / 24.0f;

    core::log(core::LogLevel::Info, "[Environment] TimeOfDay initialized, starting at {:.1f}:00",
              m_impl->current_hour);
}

void TimeOfDay::update(double dt) {
    if (m_impl->paused || m_impl->time_scale <= 0.0f) {
        return;
    }

    float old_hour = m_impl->current_hour;
    TimePeriod old_period = m_impl->current_period;

    // Advance time
    // hours_per_second = 1 / seconds_per_hour
    float hours_to_add = static_cast<float>(dt) / m_impl->seconds_per_hour * m_impl->time_scale;
    m_impl->current_hour += hours_to_add;

    // Wrap around midnight
    while (m_impl->current_hour >= 24.0f) {
        m_impl->current_hour -= 24.0f;
        m_impl->current_day++;
    }

    // Check for period change
    TimePeriod new_period = m_impl->calculate_period(m_impl->current_hour);
    if (new_period != old_period) {
        m_impl->current_period = new_period;

        // Fire period change callbacks
        for (const auto& entry : m_impl->period_callbacks) {
            if (entry.callback) {
                entry.callback(old_period, new_period);
            }
        }
    }

    // Check hour callbacks
    m_impl->check_hour_callbacks(old_hour, m_impl->current_hour);

    // Fire update callbacks
    for (const auto& entry : m_impl->update_callbacks) {
        if (entry.callback) {
            entry.callback(m_impl->current_hour);
        }
    }

    // Invalidate sun cache
    m_impl->cached_hour_for_sun = -1.0f;
}

void TimeOfDay::shutdown() {
    m_impl->period_callbacks.clear();
    m_impl->hour_callbacks.clear();
    m_impl->update_callbacks.clear();
}

void TimeOfDay::set_time(float hour) {
    // Normalize to 0-24 range
    while (hour < 0.0f) hour += 24.0f;
    while (hour >= 24.0f) hour -= 24.0f;

    float old_hour = m_impl->current_hour;
    TimePeriod old_period = m_impl->current_period;

    m_impl->current_hour = hour;
    m_impl->cached_hour_for_sun = -1.0f;  // Invalidate cache

    // Check for period change
    TimePeriod new_period = m_impl->calculate_period(m_impl->current_hour);
    if (new_period != old_period) {
        m_impl->current_period = new_period;

        for (const auto& entry : m_impl->period_callbacks) {
            if (entry.callback) {
                entry.callback(old_period, new_period);
            }
        }
    }
}

float TimeOfDay::get_time() const {
    return m_impl->current_hour;
}

float TimeOfDay::get_normalized_time() const {
    return m_impl->current_hour / 24.0f;
}

void TimeOfDay::set_time_scale(float scale) {
    m_impl->time_scale = std::max(0.0f, scale);
}

float TimeOfDay::get_time_scale() const {
    return m_impl->time_scale;
}

void TimeOfDay::pause() {
    m_impl->paused = true;
}

void TimeOfDay::resume() {
    m_impl->paused = false;
}

bool TimeOfDay::is_paused() const {
    return m_impl->paused;
}

int TimeOfDay::get_day() const {
    return m_impl->current_day;
}

void TimeOfDay::set_day(int day) {
    m_impl->current_day = day;
}

Vec3 TimeOfDay::get_sun_direction() const {
    m_impl->update_sun_position();
    return m_impl->cached_sun_direction;
}

Vec3 TimeOfDay::get_moon_direction() const {
    m_impl->update_sun_position();
    return m_impl->cached_moon_direction;
}

float TimeOfDay::get_sun_elevation() const {
    m_impl->update_sun_position();
    return m_impl->cached_sun_elevation;
}

float TimeOfDay::get_sun_azimuth() const {
    m_impl->update_sun_position();
    return m_impl->cached_sun_azimuth;
}

float TimeOfDay::get_sun_intensity() const {
    m_impl->update_sun_position();

    // Intensity based on elevation
    // Full intensity above 10 degrees, fading to 0 at horizon and below
    float elevation = m_impl->cached_sun_elevation;

    if (elevation <= 0.0f) {
        return 0.0f;
    }
    if (elevation >= 10.0f) {
        // Slight reduction as sun gets lower (atmospheric scattering)
        return 0.8f + 0.2f * std::min(1.0f, (elevation - 10.0f) / 30.0f);
    }

    // Smooth fade from horizon to 10 degrees
    return elevation / 10.0f * 0.8f;
}

bool TimeOfDay::is_sun_visible() const {
    m_impl->update_sun_position();
    return m_impl->cached_sun_elevation > -5.0f;  // Allow slight below horizon for twilight
}

TimePeriod TimeOfDay::get_current_period() const {
    return m_impl->current_period;
}

bool TimeOfDay::is_daytime() const {
    return m_impl->current_hour >= 6.0f && m_impl->current_hour < 18.0f;
}

bool TimeOfDay::is_night() const {
    return !is_daytime();
}

std::string TimeOfDay::get_time_string() const {
    int hours = static_cast<int>(m_impl->current_hour);
    int minutes = static_cast<int>((m_impl->current_hour - hours) * 60.0f);

    std::ostringstream ss;
    ss << std::setfill('0') << std::setw(2) << hours << ":"
       << std::setfill('0') << std::setw(2) << minutes;
    return ss.str();
}

std::string TimeOfDay::get_time_string_12h() const {
    int hours = static_cast<int>(m_impl->current_hour);
    int minutes = static_cast<int>((m_impl->current_hour - hours) * 60.0f);

    bool pm = hours >= 12;
    if (hours > 12) hours -= 12;
    if (hours == 0) hours = 12;

    std::ostringstream ss;
    ss << std::setfill('0') << std::setw(2) << hours << ":"
       << std::setfill('0') << std::setw(2) << minutes
       << (pm ? " PM" : " AM");
    return ss.str();
}

const TimeOfDayConfig& TimeOfDay::get_config() const {
    return m_impl->config;
}

void TimeOfDay::set_config(const TimeOfDayConfig& config) {
    m_impl->config = config;
    m_impl->seconds_per_hour = config.day_length_minutes * 60.0f / 24.0f;
}

uint32_t TimeOfDay::on_period_change(PeriodCallback callback) {
    uint32_t id = m_impl->next_callback_id++;
    m_impl->period_callbacks.push_back({id, std::move(callback)});
    return id;
}

uint32_t TimeOfDay::on_hour(float hour, HourCallback callback) {
    uint32_t id = m_impl->next_callback_id++;
    m_impl->hour_callbacks.push_back({id, hour, std::move(callback), false});
    return id;
}

uint32_t TimeOfDay::on_update(HourCallback callback) {
    uint32_t id = m_impl->next_callback_id++;
    m_impl->update_callbacks.push_back({id, std::move(callback)});
    return id;
}

void TimeOfDay::remove_callback(uint32_t id) {
    auto remove_by_id = [id](auto& vec) {
        vec.erase(
            std::remove_if(vec.begin(), vec.end(),
                [id](const auto& entry) { return entry.id == id; }),
            vec.end()
        );
    };

    remove_by_id(m_impl->period_callbacks);
    remove_by_id(m_impl->hour_callbacks);
    remove_by_id(m_impl->update_callbacks);
}

// Global instance
TimeOfDay& get_time_of_day() {
    static TimeOfDay instance;
    return instance;
}

} // namespace engine::environment
