#pragma once

#include <engine/core/math.hpp>
#include <engine/audio/sound.hpp>
#include <string>
#include <memory>

namespace engine::environment {

using namespace engine::core;

// Configuration for weather audio
struct WeatherAudioConfig {
    // Sound file paths
    std::string rain_loop_path = "audio/weather/rain_loop.wav";
    std::string rain_heavy_loop_path = "audio/weather/rain_heavy_loop.wav";
    std::string wind_loop_path = "audio/weather/wind_loop.wav";
    std::string wind_strong_loop_path = "audio/weather/wind_strong_loop.wav";
    std::string thunder_near_path = "audio/weather/thunder_near.wav";
    std::string thunder_far_path = "audio/weather/thunder_far.wav";
    std::string thunder_rumble_path = "audio/weather/thunder_rumble.wav";

    // Volume settings (master volumes, scaled by intensity)
    float rain_master_volume = 0.7f;
    float wind_master_volume = 0.5f;
    float thunder_master_volume = 0.9f;

    // Crossfade duration between light/heavy variants
    float variant_crossfade_time = 2.0f;

    // Indoor dampening (how much to reduce when indoors)
    float indoor_volume_multiplier = 0.2f;
    float indoor_lowpass_cutoff = 800.0f;  // Hz

    // Thunder timing
    float thunder_delay_min = 0.5f;   // Min seconds after lightning
    float thunder_delay_max = 3.0f;   // Max seconds after lightning
    float thunder_distance_factor = 343.0f;  // Speed of sound for delay calculation
};

// Weather audio - ambient sounds driven by weather system
class WeatherAudio {
public:
    WeatherAudio();
    ~WeatherAudio();

    // Non-copyable
    WeatherAudio(const WeatherAudio&) = delete;
    WeatherAudio& operator=(const WeatherAudio&) = delete;

    // Initialize the weather audio system
    void initialize(const WeatherAudioConfig& config = {});

    // Update each frame
    void update(double dt);

    // Shutdown
    void shutdown();

    // Configuration
    void set_config(const WeatherAudioConfig& config);
    const WeatherAudioConfig& get_config() const;

    // Volume control (0-1, scaled by weather intensity)
    void set_rain_volume(float volume);
    float get_rain_volume() const;

    void set_wind_volume(float volume);
    float get_wind_volume() const;

    void set_thunder_volume(float volume);
    float get_thunder_volume() const;

    // Set overall weather audio volume
    void set_master_volume(float volume);
    float get_master_volume() const;

    // Indoor/outdoor state
    void set_indoor(bool indoor);
    bool is_indoor() const;

    // Custom indoor dampening
    void set_indoor_dampening(float factor);  // 0-1, multiplier when indoors
    float get_indoor_dampening() const;

    // Thunder control
    void trigger_thunder(const Vec3& position);  // 3D positioned thunder
    void trigger_thunder();                       // Ambient thunder (no specific position)

    // Manually set intensity (normally driven by WeatherSystem)
    void set_rain_intensity(float intensity);    // 0-1
    void set_wind_intensity(float intensity);    // 0-1

    // Mute all weather audio (for cutscenes, etc.)
    void mute();
    void unmute();
    bool is_muted() const;

    // Enable/disable automatic sync with WeatherSystem
    void set_auto_sync(bool enabled);
    bool get_auto_sync() const;

    // Preload all weather sounds
    void preload_sounds();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// Global WeatherAudio instance accessor
WeatherAudio& get_weather_audio();

} // namespace engine::environment
