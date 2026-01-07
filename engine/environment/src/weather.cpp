#include <engine/environment/weather.hpp>
#include <engine/core/log.hpp>
#include <algorithm>
#include <random>
#include <cmath>

namespace engine::environment {

// Weather type string conversion
const char* weather_type_to_string(WeatherType type) {
    switch (type) {
        case WeatherType::Clear: return "Clear";
        case WeatherType::PartlyCloudy: return "PartlyCloudy";
        case WeatherType::Cloudy: return "Cloudy";
        case WeatherType::Overcast: return "Overcast";
        case WeatherType::LightRain: return "LightRain";
        case WeatherType::Rain: return "Rain";
        case WeatherType::HeavyRain: return "HeavyRain";
        case WeatherType::Thunderstorm: return "Thunderstorm";
        case WeatherType::LightSnow: return "LightSnow";
        case WeatherType::Snow: return "Snow";
        case WeatherType::Blizzard: return "Blizzard";
        case WeatherType::Fog: return "Fog";
        case WeatherType::DenseFog: return "DenseFog";
        case WeatherType::Sandstorm: return "Sandstorm";
        case WeatherType::Hail: return "Hail";
        default: return "Unknown";
    }
}

WeatherType weather_type_from_string(const std::string& name) {
    if (name == "Clear") return WeatherType::Clear;
    if (name == "PartlyCloudy") return WeatherType::PartlyCloudy;
    if (name == "Cloudy") return WeatherType::Cloudy;
    if (name == "Overcast") return WeatherType::Overcast;
    if (name == "LightRain") return WeatherType::LightRain;
    if (name == "Rain") return WeatherType::Rain;
    if (name == "HeavyRain") return WeatherType::HeavyRain;
    if (name == "Thunderstorm") return WeatherType::Thunderstorm;
    if (name == "LightSnow") return WeatherType::LightSnow;
    if (name == "Snow") return WeatherType::Snow;
    if (name == "Blizzard") return WeatherType::Blizzard;
    if (name == "Fog") return WeatherType::Fog;
    if (name == "DenseFog") return WeatherType::DenseFog;
    if (name == "Sandstorm") return WeatherType::Sandstorm;
    if (name == "Hail") return WeatherType::Hail;
    return WeatherType::Clear;
}

// WeatherParams lerp
WeatherParams WeatherParams::lerp(const WeatherParams& a, const WeatherParams& b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    WeatherParams result;
    result.type = t < 0.5f ? a.type : b.type;
    result.cloud_coverage = glm::mix(a.cloud_coverage, b.cloud_coverage, t);
    result.precipitation_intensity = glm::mix(a.precipitation_intensity, b.precipitation_intensity, t);
    result.precipitation_is_snow = t < 0.5f ? a.precipitation_is_snow : b.precipitation_is_snow;
    result.fog_density = glm::mix(a.fog_density, b.fog_density, t);
    result.fog_height = glm::mix(a.fog_height, b.fog_height, t);
    result.fog_color = glm::mix(a.fog_color, b.fog_color, t);
    result.wind_speed = glm::mix(a.wind_speed, b.wind_speed, t);
    result.wind_direction = glm::normalize(glm::mix(a.wind_direction, b.wind_direction, t));
    result.wind_gustiness = glm::mix(a.wind_gustiness, b.wind_gustiness, t);
    result.wetness = glm::mix(a.wetness, b.wetness, t);
    result.snow_accumulation = glm::mix(a.snow_accumulation, b.snow_accumulation, t);
    result.thunder_frequency = glm::mix(a.thunder_frequency, b.thunder_frequency, t);
    result.lightning_intensity = glm::mix(a.lightning_intensity, b.lightning_intensity, t);
    result.rain_volume = glm::mix(a.rain_volume, b.rain_volume, t);
    result.wind_volume = glm::mix(a.wind_volume, b.wind_volume, t);
    result.thunder_volume = glm::mix(a.thunder_volume, b.thunder_volume, t);
    result.temperature = glm::mix(a.temperature, b.temperature, t);
    result.visibility = glm::mix(a.visibility, b.visibility, t);
    return result;
}

// Weather presets
WeatherParams get_weather_preset(WeatherType type) {
    WeatherParams p;
    p.type = type;

    switch (type) {
        case WeatherType::Clear:
            p.cloud_coverage = 0.1f;
            p.visibility = 10000.0f;
            p.temperature = 22.0f;
            break;

        case WeatherType::PartlyCloudy:
            p.cloud_coverage = 0.4f;
            p.visibility = 8000.0f;
            p.wind_speed = 3.0f;
            break;

        case WeatherType::Cloudy:
            p.cloud_coverage = 0.7f;
            p.visibility = 5000.0f;
            p.wind_speed = 5.0f;
            break;

        case WeatherType::Overcast:
            p.cloud_coverage = 0.95f;
            p.fog_density = 0.1f;
            p.visibility = 3000.0f;
            p.wind_speed = 4.0f;
            break;

        case WeatherType::LightRain:
            p.cloud_coverage = 0.8f;
            p.precipitation_intensity = 0.3f;
            p.fog_density = 0.15f;
            p.wetness = 0.4f;
            p.visibility = 2000.0f;
            p.wind_speed = 5.0f;
            p.rain_volume = 0.4f;
            p.temperature = 15.0f;
            break;

        case WeatherType::Rain:
            p.cloud_coverage = 0.9f;
            p.precipitation_intensity = 0.6f;
            p.fog_density = 0.2f;
            p.wetness = 0.8f;
            p.visibility = 1000.0f;
            p.wind_speed = 8.0f;
            p.rain_volume = 0.7f;
            p.wind_volume = 0.3f;
            p.temperature = 12.0f;
            break;

        case WeatherType::HeavyRain:
            p.cloud_coverage = 1.0f;
            p.precipitation_intensity = 1.0f;
            p.fog_density = 0.3f;
            p.wetness = 1.0f;
            p.visibility = 500.0f;
            p.wind_speed = 12.0f;
            p.wind_gustiness = 0.5f;
            p.rain_volume = 1.0f;
            p.wind_volume = 0.5f;
            p.temperature = 10.0f;
            break;

        case WeatherType::Thunderstorm:
            p.cloud_coverage = 1.0f;
            p.precipitation_intensity = 0.9f;
            p.fog_density = 0.25f;
            p.wetness = 1.0f;
            p.visibility = 600.0f;
            p.wind_speed = 15.0f;
            p.wind_gustiness = 0.7f;
            p.thunder_frequency = 3.0f;  // 3 strikes per minute
            p.lightning_intensity = 1.0f;
            p.rain_volume = 0.9f;
            p.wind_volume = 0.7f;
            p.thunder_volume = 0.9f;
            p.temperature = 8.0f;
            break;

        case WeatherType::LightSnow:
            p.cloud_coverage = 0.85f;
            p.precipitation_intensity = 0.3f;
            p.precipitation_is_snow = true;
            p.fog_density = 0.1f;
            p.snow_accumulation = 0.2f;
            p.visibility = 2000.0f;
            p.wind_speed = 4.0f;
            p.temperature = -2.0f;
            break;

        case WeatherType::Snow:
            p.cloud_coverage = 0.95f;
            p.precipitation_intensity = 0.6f;
            p.precipitation_is_snow = true;
            p.fog_density = 0.2f;
            p.snow_accumulation = 0.6f;
            p.visibility = 800.0f;
            p.wind_speed = 6.0f;
            p.wind_volume = 0.3f;
            p.temperature = -5.0f;
            break;

        case WeatherType::Blizzard:
            p.cloud_coverage = 1.0f;
            p.precipitation_intensity = 1.0f;
            p.precipitation_is_snow = true;
            p.fog_density = 0.5f;
            p.snow_accumulation = 1.0f;
            p.visibility = 100.0f;
            p.wind_speed = 20.0f;
            p.wind_gustiness = 0.8f;
            p.wind_volume = 1.0f;
            p.temperature = -15.0f;
            break;

        case WeatherType::Fog:
            p.cloud_coverage = 0.6f;
            p.fog_density = 0.6f;
            p.fog_height = 50.0f;
            p.visibility = 200.0f;
            p.wetness = 0.3f;
            p.temperature = 10.0f;
            break;

        case WeatherType::DenseFog:
            p.cloud_coverage = 0.7f;
            p.fog_density = 0.9f;
            p.fog_height = 100.0f;
            p.visibility = 50.0f;
            p.wetness = 0.5f;
            p.temperature = 8.0f;
            break;

        case WeatherType::Sandstorm:
            p.cloud_coverage = 0.3f;
            p.fog_density = 0.7f;
            p.fog_color = Vec3(0.8f, 0.7f, 0.5f);  // Sandy color
            p.visibility = 100.0f;
            p.wind_speed = 25.0f;
            p.wind_gustiness = 0.9f;
            p.wind_volume = 1.0f;
            p.temperature = 35.0f;
            break;

        case WeatherType::Hail:
            p.cloud_coverage = 1.0f;
            p.precipitation_intensity = 0.7f;
            p.fog_density = 0.15f;
            p.visibility = 800.0f;
            p.wind_speed = 10.0f;
            p.wind_gustiness = 0.4f;
            p.rain_volume = 0.6f;
            p.temperature = 5.0f;
            break;
    }

    return p;
}

// Implementation struct
struct WeatherSystem::Impl {
    bool initialized = false;

    WeatherParams current_params;
    WeatherParams target_params;
    WeatherParams start_params;  // For lerping from

    float transition_progress = 1.0f;
    float transition_duration = 0.0f;

    // Thunder timing
    float thunder_timer = 0.0f;
    float next_thunder_time = 0.0f;
    std::mt19937 rng{std::random_device{}()};

    // Weather sequence
    std::vector<WeatherSequenceEntry> sequence;
    size_t sequence_index = 0;
    float sequence_timer = 0.0f;
    bool sequence_loop = false;
    bool sequence_active = false;

    // Callbacks
    struct WeatherChangeCallbackEntry {
        uint32_t id;
        WeatherChangeCallback callback;
    };
    struct ThunderCallbackEntry {
        uint32_t id;
        ThunderCallback callback;
    };
    std::vector<WeatherChangeCallbackEntry> weather_callbacks;
    std::vector<ThunderCallbackEntry> thunder_callbacks;
    uint32_t next_callback_id = 1;

    void schedule_next_thunder() {
        if (current_params.thunder_frequency <= 0.0f) {
            next_thunder_time = -1.0f;
            return;
        }
        // Random interval based on frequency
        float avg_interval = 60.0f / current_params.thunder_frequency;
        std::uniform_real_distribution<float> dist(avg_interval * 0.5f, avg_interval * 1.5f);
        next_thunder_time = thunder_timer + dist(rng);
    }

    void fire_thunder(const Vec3& position, float intensity) {
        for (const auto& entry : thunder_callbacks) {
            if (entry.callback) {
                entry.callback(position, intensity);
            }
        }
    }
};

WeatherSystem::WeatherSystem() : m_impl(std::make_unique<Impl>()) {}

WeatherSystem::~WeatherSystem() = default;

void WeatherSystem::initialize() {
    m_impl->current_params = get_weather_preset(WeatherType::Clear);
    m_impl->target_params = m_impl->current_params;
    m_impl->start_params = m_impl->current_params;
    m_impl->initialized = true;

    core::log(core::LogLevel::Info, "[Environment] WeatherSystem initialized");
}

void WeatherSystem::update(double dt) {
    if (!m_impl->initialized) return;

    float fdt = static_cast<float>(dt);

    // Handle transition
    if (m_impl->transition_progress < 1.0f && m_impl->transition_duration > 0.0f) {
        m_impl->transition_progress += fdt / m_impl->transition_duration;
        m_impl->transition_progress = std::min(m_impl->transition_progress, 1.0f);

        m_impl->current_params = WeatherParams::lerp(
            m_impl->start_params, m_impl->target_params, m_impl->transition_progress);

        // If transition just completed
        if (m_impl->transition_progress >= 1.0f) {
            m_impl->current_params = m_impl->target_params;
        }
    }

    // Handle weather sequence
    if (m_impl->sequence_active && !m_impl->sequence.empty()) {
        m_impl->sequence_timer += fdt;

        const auto& current_entry = m_impl->sequence[m_impl->sequence_index];

        if (m_impl->sequence_timer >= current_entry.duration) {
            m_impl->sequence_timer = 0.0f;
            m_impl->sequence_index++;

            if (m_impl->sequence_index >= m_impl->sequence.size()) {
                if (m_impl->sequence_loop) {
                    m_impl->sequence_index = 0;
                } else {
                    m_impl->sequence_active = false;
                }
            }

            if (m_impl->sequence_active) {
                const auto& next_entry = m_impl->sequence[m_impl->sequence_index];
                set_weather(next_entry.type, next_entry.transition_time);
            }
        }
    }

    // Handle thunder
    if (m_impl->current_params.thunder_frequency > 0.0f) {
        m_impl->thunder_timer += fdt;

        if (m_impl->next_thunder_time < 0.0f) {
            m_impl->schedule_next_thunder();
        }

        if (m_impl->thunder_timer >= m_impl->next_thunder_time) {
            // Generate random position (within some radius of origin)
            std::uniform_real_distribution<float> pos_dist(-500.0f, 500.0f);
            std::uniform_real_distribution<float> height_dist(200.0f, 500.0f);
            std::uniform_real_distribution<float> intensity_dist(0.7f, 1.0f);

            Vec3 thunder_pos{
                pos_dist(m_impl->rng),
                height_dist(m_impl->rng),
                pos_dist(m_impl->rng)
            };
            float intensity = intensity_dist(m_impl->rng) * m_impl->current_params.lightning_intensity;

            m_impl->fire_thunder(thunder_pos, intensity);
            m_impl->schedule_next_thunder();
        }
    }
}

void WeatherSystem::shutdown() {
    m_impl->weather_callbacks.clear();
    m_impl->thunder_callbacks.clear();
    m_impl->sequence.clear();
    m_impl->initialized = false;
}

void WeatherSystem::set_weather(WeatherType type, float transition_time) {
    set_weather(get_weather_preset(type), transition_time);
}

void WeatherSystem::set_weather(const WeatherParams& params, float transition_time) {
    WeatherType old_type = m_impl->current_params.type;
    WeatherType new_type = params.type;

    m_impl->start_params = m_impl->current_params;
    m_impl->target_params = params;

    if (transition_time <= 0.0f) {
        m_impl->current_params = params;
        m_impl->transition_progress = 1.0f;
    } else {
        m_impl->transition_duration = transition_time;
        m_impl->transition_progress = 0.0f;
    }

    // Fire weather change callbacks
    if (old_type != new_type) {
        for (const auto& entry : m_impl->weather_callbacks) {
            if (entry.callback) {
                entry.callback(old_type, new_type);
            }
        }
    }

    // Reset thunder scheduling
    m_impl->schedule_next_thunder();
}

WeatherType WeatherSystem::get_current_weather() const {
    return m_impl->current_params.type;
}

const WeatherParams& WeatherSystem::get_current_params() const {
    return m_impl->current_params;
}

WeatherType WeatherSystem::get_target_weather() const {
    return m_impl->target_params.type;
}

const WeatherParams& WeatherSystem::get_target_params() const {
    return m_impl->target_params;
}

float WeatherSystem::get_transition_progress() const {
    return m_impl->transition_progress;
}

bool WeatherSystem::is_transitioning() const {
    return m_impl->transition_progress < 1.0f;
}

void WeatherSystem::cancel_transition() {
    m_impl->target_params = m_impl->current_params;
    m_impl->transition_progress = 1.0f;
}

void WeatherSystem::set_weather_immediate(WeatherType type) {
    set_weather_immediate(get_weather_preset(type));
}

void WeatherSystem::set_weather_immediate(const WeatherParams& params) {
    m_impl->current_params = params;
    m_impl->target_params = params;
    m_impl->start_params = params;
    m_impl->transition_progress = 1.0f;
}

bool WeatherSystem::is_raining() const {
    return m_impl->current_params.precipitation_intensity > 0.0f &&
           !m_impl->current_params.precipitation_is_snow;
}

bool WeatherSystem::is_snowing() const {
    return m_impl->current_params.precipitation_intensity > 0.0f &&
           m_impl->current_params.precipitation_is_snow;
}

bool WeatherSystem::is_foggy() const {
    return m_impl->current_params.fog_density > 0.3f;
}

bool WeatherSystem::is_stormy() const {
    return m_impl->current_params.thunder_frequency > 0.0f;
}

float WeatherSystem::get_wetness() const {
    return m_impl->current_params.wetness;
}

float WeatherSystem::get_precipitation() const {
    return m_impl->current_params.precipitation_intensity;
}

float WeatherSystem::get_visibility() const {
    return m_impl->current_params.visibility;
}

Vec3 WeatherSystem::get_wind_direction() const {
    return m_impl->current_params.wind_direction;
}

float WeatherSystem::get_wind_speed() const {
    return m_impl->current_params.wind_speed;
}

void WeatherSystem::set_random_weather(float transition_time) {
    std::uniform_int_distribution<int> dist(0, 12);  // Exclude sandstorm/hail for generic use
    WeatherType type = static_cast<WeatherType>(dist(m_impl->rng));
    set_weather(type, transition_time);
}

void WeatherSystem::set_weather_sequence(const std::vector<WeatherSequenceEntry>& sequence, bool loop) {
    m_impl->sequence = sequence;
    m_impl->sequence_loop = loop;
    m_impl->sequence_index = 0;
    m_impl->sequence_timer = 0.0f;
    m_impl->sequence_active = !sequence.empty();

    if (m_impl->sequence_active) {
        set_weather(sequence[0].type, sequence[0].transition_time);
    }
}

void WeatherSystem::clear_weather_sequence() {
    m_impl->sequence.clear();
    m_impl->sequence_active = false;
}

bool WeatherSystem::is_sequence_active() const {
    return m_impl->sequence_active;
}

uint32_t WeatherSystem::on_weather_change(WeatherChangeCallback callback) {
    uint32_t id = m_impl->next_callback_id++;
    m_impl->weather_callbacks.push_back({id, std::move(callback)});
    return id;
}

uint32_t WeatherSystem::on_thunder_strike(ThunderCallback callback) {
    uint32_t id = m_impl->next_callback_id++;
    m_impl->thunder_callbacks.push_back({id, std::move(callback)});
    return id;
}

void WeatherSystem::remove_callback(uint32_t id) {
    auto remove_by_id = [id](auto& vec) {
        vec.erase(
            std::remove_if(vec.begin(), vec.end(),
                [id](const auto& entry) { return entry.id == id; }),
            vec.end()
        );
    };

    remove_by_id(m_impl->weather_callbacks);
    remove_by_id(m_impl->thunder_callbacks);
}

// Global instance
WeatherSystem& get_weather_system() {
    static WeatherSystem instance;
    return instance;
}

} // namespace engine::environment
