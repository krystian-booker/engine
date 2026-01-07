#pragma once

#include <engine/core/math.hpp>
#include <engine/scene/entity.hpp>
#include <memory>

namespace engine::scene {
    class World;
}

namespace engine::render {
    class RenderPipeline;
}

namespace engine::environment {

using namespace engine::core;

// Configuration for weather visual effects
struct WeatherEffectsConfig {
    // Rain particles
    int max_rain_particles = 10000;
    float rain_particle_size = 0.02f;
    float rain_particle_length = 0.3f;     // Stretch factor for motion blur
    Vec3 rain_color{0.6f, 0.65f, 0.75f};
    float rain_opacity = 0.5f;

    // Snow particles
    int max_snow_particles = 5000;
    float snow_particle_size = 0.03f;
    Vec3 snow_color{0.95f, 0.95f, 1.0f};
    float snow_opacity = 0.8f;
    float snow_turbulence = 0.5f;          // How much wind affects snow

    // Rain on screen (camera lens effect)
    float screen_droplet_intensity = 0.5f;
    float screen_droplet_size = 0.02f;

    // Lightning flash
    float lightning_flash_duration = 0.15f;
    Vec3 lightning_color{0.9f, 0.95f, 1.0f};
    float lightning_intensity = 3.0f;       // Brightness multiplier

    // Wet surfaces
    float wet_roughness_reduction = 0.3f;   // How much to reduce PBR roughness
    float wet_specular_boost = 0.2f;        // How much to increase specular
    float dry_speed = 0.1f;                 // How fast surfaces dry (wetness/second)

    // Fog override settings
    Vec3 fog_color{0.7f, 0.75f, 0.8f};
    float fog_start_distance = 10.0f;
    float fog_end_distance = 500.0f;
};

// Weather effects - visual effects driven by weather system
class WeatherEffects {
public:
    WeatherEffects();
    ~WeatherEffects();

    // Non-copyable
    WeatherEffects(const WeatherEffects&) = delete;
    WeatherEffects& operator=(const WeatherEffects&) = delete;

    // Initialize with world and render pipeline references
    void initialize(scene::World& world, render::RenderPipeline& pipeline,
                    const WeatherEffectsConfig& config = {});

    // Update each frame
    void update(double dt);

    // Shutdown
    void shutdown();

    // Configuration
    void set_config(const WeatherEffectsConfig& config);
    const WeatherEffectsConfig& get_config() const;

    // Particle system control (driven by WeatherSystem, but can be overridden)
    void set_rain_enabled(bool enabled);
    bool is_rain_enabled() const;
    void set_rain_intensity(float intensity);  // 0-1, controls spawn rate
    float get_rain_intensity() const;

    void set_snow_enabled(bool enabled);
    bool is_snow_enabled() const;
    void set_snow_intensity(float intensity);
    float get_snow_intensity() const;

    // Set particle spawn area (typically centered on camera)
    void set_particle_bounds(const Vec3& center, const Vec3& extents);

    // Material wetness (global override for all PBR materials)
    void set_surface_wetness(float wetness);  // 0-1
    float get_surface_wetness() const;

    // Fog override (overrides post-processing fog settings)
    void set_fog_enabled(bool enabled);
    bool is_fog_enabled() const;
    void set_fog_density(float density);
    void set_fog_color(const Vec3& color);
    void set_fog_height(float height);

    // Screen effects
    void set_rain_on_screen_enabled(bool enabled);
    bool is_rain_on_screen_enabled() const;

    // Lightning flash (triggered by WeatherSystem on thunder)
    void trigger_lightning_flash(float intensity = 1.0f);
    void trigger_lightning_flash_at(const Vec3& position, float intensity = 1.0f);

    // Get particle emitter entities (for advanced manipulation)
    scene::Entity get_rain_emitter() const;
    scene::Entity get_snow_emitter() const;

    // Enable/disable automatic sync with WeatherSystem
    void set_auto_sync(bool enabled);
    bool get_auto_sync() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// Global WeatherEffects instance accessor
WeatherEffects& get_weather_effects();

} // namespace engine::environment
