#include <engine/environment/weather_audio.hpp>
#include <engine/environment/weather.hpp>
#include <engine/audio/audio_engine.hpp>
#include <engine/core/log.hpp>
#include <cmath>

namespace engine::environment {

// Implementation struct
struct WeatherAudio::Impl {
    bool initialized = false;
    bool auto_sync = true;
    bool muted = false;
    bool indoor = false;

    WeatherAudioConfig config;

    // Sound handles
    audio::SoundHandle rain_loop;
    audio::SoundHandle rain_heavy_loop;
    audio::SoundHandle wind_loop;
    audio::SoundHandle wind_strong_loop;
    std::vector<audio::SoundHandle> thunder_sounds;

    // Current volumes (0-1)
    float rain_volume = 0.0f;
    float wind_volume = 0.0f;
    float thunder_volume = 0.9f;
    float master_volume = 1.0f;

    // Current intensities (for crossfading between light/heavy variants)
    float rain_intensity = 0.0f;
    float wind_intensity = 0.0f;

    // Indoor dampening
    float indoor_dampening = 0.2f;

    // Target volumes for smooth transitions
    float target_rain_volume = 0.0f;
    float target_wind_volume = 0.0f;

    float get_effective_volume(float base_volume) const {
        float vol = base_volume * master_volume;
        if (indoor) {
            vol *= indoor_dampening;
        }
        if (muted) {
            vol = 0.0f;
        }
        return vol;
    }
};

WeatherAudio::WeatherAudio() : m_impl(std::make_unique<Impl>()) {}

WeatherAudio::~WeatherAudio() = default;

void WeatherAudio::initialize(const WeatherAudioConfig& config) {
    m_impl->config = config;

    // Load sounds
    auto& audio = audio::get_audio_engine();

    // Note: In a real implementation, you'd check if files exist before loading
    // and handle errors gracefully

    m_impl->rain_loop = audio.load_sound(config.rain_loop_path);
    m_impl->rain_heavy_loop = audio.load_sound(config.rain_heavy_loop_path);
    m_impl->wind_loop = audio.load_sound(config.wind_loop_path);
    m_impl->wind_strong_loop = audio.load_sound(config.wind_strong_loop_path);

    // Load thunder variants
    m_impl->thunder_sounds.push_back(audio.load_sound(config.thunder_near_path));
    m_impl->thunder_sounds.push_back(audio.load_sound(config.thunder_far_path));
    m_impl->thunder_sounds.push_back(audio.load_sound(config.thunder_rumble_path));

    m_impl->initialized = true;

    core::log(core::LogLevel::Info, "[Environment] WeatherAudio initialized");
}

void WeatherAudio::update(double dt) {
    if (!m_impl->initialized) return;

    float fdt = static_cast<float>(dt);

    // Auto-sync with WeatherSystem
    if (m_impl->auto_sync) {
        const WeatherParams& params = get_weather_system().get_current_params();

        // Set target volumes based on weather
        m_impl->target_rain_volume = params.rain_volume * m_impl->config.rain_master_volume;
        m_impl->target_wind_volume = params.wind_volume * m_impl->config.wind_master_volume;
        m_impl->rain_intensity = params.precipitation_intensity;
        m_impl->wind_intensity = std::min(1.0f, params.wind_speed / 20.0f);
    }

    // Smooth volume transitions
    float blend_speed = 1.0f / m_impl->config.variant_crossfade_time;

    if (m_impl->rain_volume < m_impl->target_rain_volume) {
        m_impl->rain_volume = std::min(m_impl->target_rain_volume,
            m_impl->rain_volume + fdt * blend_speed);
    } else if (m_impl->rain_volume > m_impl->target_rain_volume) {
        m_impl->rain_volume = std::max(m_impl->target_rain_volume,
            m_impl->rain_volume - fdt * blend_speed);
    }

    if (m_impl->wind_volume < m_impl->target_wind_volume) {
        m_impl->wind_volume = std::min(m_impl->target_wind_volume,
            m_impl->wind_volume + fdt * blend_speed);
    } else if (m_impl->wind_volume > m_impl->target_wind_volume) {
        m_impl->wind_volume = std::max(m_impl->target_wind_volume,
            m_impl->wind_volume - fdt * blend_speed);
    }

    // Apply volumes to sounds
    auto& audio = audio::get_audio_engine();

    // Rain sounds - crossfade between light and heavy based on intensity
    float rain_eff = m_impl->get_effective_volume(m_impl->rain_volume);
    if (rain_eff > 0.01f) {
        float light_vol = rain_eff * (1.0f - m_impl->rain_intensity);
        float heavy_vol = rain_eff * m_impl->rain_intensity;

        audio.set_volume(m_impl->rain_loop, light_vol);
        audio.set_volume(m_impl->rain_heavy_loop, heavy_vol);

        // Start playing if not already
        if (!audio.is_sound_playing(m_impl->rain_loop) && light_vol > 0.01f) {
            audio.play_sound(m_impl->rain_loop, {.volume = light_vol, .loop = true});
        }
        if (!audio.is_sound_playing(m_impl->rain_heavy_loop) && heavy_vol > 0.01f) {
            audio.play_sound(m_impl->rain_heavy_loop, {.volume = heavy_vol, .loop = true});
        }
    } else {
        audio.stop_sound(m_impl->rain_loop);
        audio.stop_sound(m_impl->rain_heavy_loop);
    }

    // Wind sounds - crossfade between light and strong
    float wind_eff = m_impl->get_effective_volume(m_impl->wind_volume);
    if (wind_eff > 0.01f) {
        float light_vol = wind_eff * (1.0f - m_impl->wind_intensity);
        float strong_vol = wind_eff * m_impl->wind_intensity;

        audio.set_volume(m_impl->wind_loop, light_vol);
        audio.set_volume(m_impl->wind_strong_loop, strong_vol);

        if (!audio.is_sound_playing(m_impl->wind_loop) && light_vol > 0.01f) {
            audio.play_sound(m_impl->wind_loop, {.volume = light_vol, .loop = true});
        }
        if (!audio.is_sound_playing(m_impl->wind_strong_loop) && strong_vol > 0.01f) {
            audio.play_sound(m_impl->wind_strong_loop, {.volume = strong_vol, .loop = true});
        }
    } else {
        audio.stop_sound(m_impl->wind_loop);
        audio.stop_sound(m_impl->wind_strong_loop);
    }

    // Apply indoor lowpass filter if needed
    if (m_impl->indoor && !m_impl->muted) {
        auto sfx_bus = audio.get_bus(audio::BuiltinBus::SFX);
        audio.set_bus_lowpass(sfx_bus, m_impl->config.indoor_lowpass_cutoff, true);
    }
}

void WeatherAudio::shutdown() {
    if (!m_impl->initialized) return;

    auto& audio = audio::get_audio_engine();

    // Stop and unload all sounds
    audio.stop_sound(m_impl->rain_loop);
    audio.stop_sound(m_impl->rain_heavy_loop);
    audio.stop_sound(m_impl->wind_loop);
    audio.stop_sound(m_impl->wind_strong_loop);

    audio.unload_sound(m_impl->rain_loop);
    audio.unload_sound(m_impl->rain_heavy_loop);
    audio.unload_sound(m_impl->wind_loop);
    audio.unload_sound(m_impl->wind_strong_loop);

    for (auto& handle : m_impl->thunder_sounds) {
        audio.unload_sound(handle);
    }
    m_impl->thunder_sounds.clear();

    m_impl->initialized = false;
}

void WeatherAudio::set_config(const WeatherAudioConfig& config) {
    m_impl->config = config;
}

const WeatherAudioConfig& WeatherAudio::get_config() const {
    return m_impl->config;
}

void WeatherAudio::set_rain_volume(float volume) {
    m_impl->target_rain_volume = std::clamp(volume, 0.0f, 1.0f) * m_impl->config.rain_master_volume;
}

float WeatherAudio::get_rain_volume() const {
    return m_impl->rain_volume;
}

void WeatherAudio::set_wind_volume(float volume) {
    m_impl->target_wind_volume = std::clamp(volume, 0.0f, 1.0f) * m_impl->config.wind_master_volume;
}

float WeatherAudio::get_wind_volume() const {
    return m_impl->wind_volume;
}

void WeatherAudio::set_thunder_volume(float volume) {
    m_impl->thunder_volume = std::clamp(volume, 0.0f, 1.0f);
}

float WeatherAudio::get_thunder_volume() const {
    return m_impl->thunder_volume;
}

void WeatherAudio::set_master_volume(float volume) {
    m_impl->master_volume = std::clamp(volume, 0.0f, 1.0f);
}

float WeatherAudio::get_master_volume() const {
    return m_impl->master_volume;
}

void WeatherAudio::set_indoor(bool indoor) {
    m_impl->indoor = indoor;

    // Clear lowpass if going outdoors
    if (!indoor) {
        auto& audio = audio::get_audio_engine();
        auto sfx_bus = audio.get_bus(audio::BuiltinBus::SFX);
        audio.set_bus_lowpass(sfx_bus, 20000.0f, false);
    }
}

bool WeatherAudio::is_indoor() const {
    return m_impl->indoor;
}

void WeatherAudio::set_indoor_dampening(float factor) {
    m_impl->indoor_dampening = std::clamp(factor, 0.0f, 1.0f);
}

float WeatherAudio::get_indoor_dampening() const {
    return m_impl->indoor_dampening;
}

void WeatherAudio::trigger_thunder(const Vec3& position) {
    if (m_impl->muted || m_impl->thunder_sounds.empty()) return;

    auto& audio = audio::get_audio_engine();

    // Select random thunder sound
    size_t idx = static_cast<size_t>(rand()) % m_impl->thunder_sounds.size();
    auto sound = m_impl->thunder_sounds[idx];

    float volume = m_impl->get_effective_volume(m_impl->thunder_volume * m_impl->config.thunder_master_volume);

    audio.play_sound_3d(sound, position, {.volume = volume});
}

void WeatherAudio::trigger_thunder() {
    if (m_impl->muted || m_impl->thunder_sounds.empty()) return;

    auto& audio = audio::get_audio_engine();

    // Select random thunder sound
    size_t idx = static_cast<size_t>(rand()) % m_impl->thunder_sounds.size();
    auto sound = m_impl->thunder_sounds[idx];

    float volume = m_impl->get_effective_volume(m_impl->thunder_volume * m_impl->config.thunder_master_volume);

    audio.play_sound(sound, {.volume = volume});
}

void WeatherAudio::set_rain_intensity(float intensity) {
    m_impl->rain_intensity = std::clamp(intensity, 0.0f, 1.0f);
}

void WeatherAudio::set_wind_intensity(float intensity) {
    m_impl->wind_intensity = std::clamp(intensity, 0.0f, 1.0f);
}

void WeatherAudio::mute() {
    m_impl->muted = true;
}

void WeatherAudio::unmute() {
    m_impl->muted = false;
}

bool WeatherAudio::is_muted() const {
    return m_impl->muted;
}

void WeatherAudio::set_auto_sync(bool enabled) {
    m_impl->auto_sync = enabled;
}

bool WeatherAudio::get_auto_sync() const {
    return m_impl->auto_sync;
}

void WeatherAudio::preload_sounds() {
    // Sounds are loaded in initialize()
    // This method could be used to pre-warm audio buffers
}

// Global instance
WeatherAudio& get_weather_audio() {
    static WeatherAudio instance;
    return instance;
}

} // namespace engine::environment
