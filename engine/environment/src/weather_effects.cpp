#include <engine/environment/weather_effects.hpp>
#include <engine/environment/weather.hpp>
#include <engine/scene/world.hpp>
#include <engine/core/log.hpp>
#include <engine/render/particle_system.hpp>

namespace engine::environment {

// Helper to create rain particle emitter configuration
static render::ParticleEmitterConfig create_rain_config(const WeatherEffectsConfig& config) {
    render::ParticleEmitterConfig emitter;

    emitter.max_particles = static_cast<uint32_t>(config.max_rain_particles);
    emitter.emission_rate = 0.0f; // Will be set based on intensity

    // Box emission covering area above camera
    emitter.emission_shape.shape = render::EmissionShape::Box;
    emitter.emission_shape.size = Vec3{50.0f, 1.0f, 50.0f}; // Wide, thin spawn area
    emitter.emission_shape.emit_from_surface = false;

    // Rain falls fast
    emitter.lifetime = 1.5f;
    emitter.lifetime_variance = 0.3f;
    emitter.initial_velocity = Vec3{0.0f, -25.0f, 0.0f}; // Fast downward
    emitter.velocity_variance = Vec3{1.0f, 2.0f, 1.0f};  // Slight variation

    // Small, elongated particles (size is base, we stretch via angular velocity trick)
    emitter.initial_size = config.rain_particle_size;
    emitter.size_variance = 0.005f;

    // Rain doesn't rotate much
    emitter.initial_rotation = 0.0f;
    emitter.rotation_variance = 0.1f;
    emitter.angular_velocity = 0.0f;
    emitter.angular_velocity_variance = 0.0f;

    // Color: use rain color with fade at end of life
    render::ColorGradient color_gradient;
    color_gradient.keys.clear();
    color_gradient.keys.push_back({Vec4{config.rain_color.x, config.rain_color.y, config.rain_color.z, config.rain_opacity}, 0.0f});
    color_gradient.keys.push_back({Vec4{config.rain_color.x, config.rain_color.y, config.rain_color.z, config.rain_opacity}, 0.8f});
    color_gradient.keys.push_back({Vec4{config.rain_color.x, config.rain_color.y, config.rain_color.z, 0.0f}, 1.0f});
    emitter.color_over_life = color_gradient;

    // Size stays constant, speed increases slightly (acceleration)
    emitter.size_over_life = render::Curve(); // Default constant
    render::Curve speed_curve;
    speed_curve.keys.clear();
    speed_curve.keys.push_back({1.0f, 0.0f});
    speed_curve.keys.push_back({1.5f, 1.0f}); // Speed up as it falls
    emitter.speed_over_life = speed_curve;

    // Gravity acceleration
    emitter.gravity = Vec3{0.0f, -5.0f, 0.0f}; // Additional gravity on top of velocity
    emitter.drag = 0.0f;

    // Rendering
    emitter.blend_mode = render::ParticleBlendMode::Alpha;
    emitter.face_camera = true;
    emitter.soft_particles = true;
    emitter.soft_particle_distance = 0.3f;

    emitter.enabled = true;
    emitter.loop = true;
    emitter.world_space = true;
    emitter.sort_by_depth = false; // Rain doesn't need sorting

    return emitter;
}

// Helper to create snow particle emitter configuration
static render::ParticleEmitterConfig create_snow_config(const WeatherEffectsConfig& config) {
    render::ParticleEmitterConfig emitter;

    emitter.max_particles = static_cast<uint32_t>(config.max_snow_particles);
    emitter.emission_rate = 0.0f; // Will be set based on intensity

    // Box emission covering area above camera
    emitter.emission_shape.shape = render::EmissionShape::Box;
    emitter.emission_shape.size = Vec3{60.0f, 2.0f, 60.0f}; // Wide spawn area
    emitter.emission_shape.emit_from_surface = false;

    // Snow falls slowly with horizontal drift
    emitter.lifetime = 8.0f;  // Long lifetime for slow fall
    emitter.lifetime_variance = 2.0f;
    emitter.initial_velocity = Vec3{0.0f, -2.0f, 0.0f}; // Slow downward
    emitter.velocity_variance = Vec3{1.5f * config.snow_turbulence, 0.5f, 1.5f * config.snow_turbulence};

    // Larger, softer particles
    emitter.initial_size = config.snow_particle_size;
    emitter.size_variance = 0.01f;

    // Snow tumbles
    emitter.initial_rotation = 0.0f;
    emitter.rotation_variance = 3.14159f; // Full random rotation
    emitter.angular_velocity = 1.0f;
    emitter.angular_velocity_variance = 2.0f;

    // Color: white snow, slight fade at edges of life
    render::ColorGradient color_gradient;
    color_gradient.keys.clear();
    color_gradient.keys.push_back({Vec4{config.snow_color.x, config.snow_color.y, config.snow_color.z, 0.0f}, 0.0f});
    color_gradient.keys.push_back({Vec4{config.snow_color.x, config.snow_color.y, config.snow_color.z, config.snow_opacity}, 0.1f});
    color_gradient.keys.push_back({Vec4{config.snow_color.x, config.snow_color.y, config.snow_color.z, config.snow_opacity}, 0.85f});
    color_gradient.keys.push_back({Vec4{config.snow_color.x, config.snow_color.y, config.snow_color.z, 0.0f}, 1.0f});
    emitter.color_over_life = color_gradient;

    // Size grows slightly, then shrinks (melting effect)
    render::Curve size_curve;
    size_curve.keys.clear();
    size_curve.keys.push_back({0.8f, 0.0f});
    size_curve.keys.push_back({1.2f, 0.3f});
    size_curve.keys.push_back({0.6f, 1.0f});
    emitter.size_over_life = size_curve;

    // Speed varies (turbulent motion)
    render::Curve speed_curve;
    speed_curve.keys.clear();
    speed_curve.keys.push_back({1.0f, 0.0f});
    speed_curve.keys.push_back({0.8f, 0.5f});
    speed_curve.keys.push_back({1.2f, 1.0f});
    emitter.speed_over_life = speed_curve;

    // Light gravity for gentle fall
    emitter.gravity = Vec3{0.0f, -0.5f, 0.0f};
    emitter.drag = 0.1f; // Slight air resistance

    // Rendering
    emitter.blend_mode = render::ParticleBlendMode::Alpha;
    emitter.face_camera = true;
    emitter.soft_particles = true;
    emitter.soft_particle_distance = 0.5f;

    emitter.enabled = true;
    emitter.loop = true;
    emitter.world_space = true;
    emitter.sort_by_depth = false;

    return emitter;
}

// Implementation struct
struct WeatherEffects::Impl {
    bool initialized = false;
    bool auto_sync = true;
    scene::World* world = nullptr;
    render::RenderPipeline* pipeline = nullptr;

    WeatherEffectsConfig config;

    // Particle emitter entities
    scene::Entity rain_emitter;
    scene::Entity snow_emitter;

    // Particle system configurations
    render::ParticleEmitterConfig rain_config;
    render::ParticleEmitterConfig snow_config;

    // Particle system runtimes (owned by ParticleSystem, we just hold pointers)
    render::ParticleEmitterRuntime* rain_runtime = nullptr;
    render::ParticleEmitterRuntime* snow_runtime = nullptr;

    // Reference to particle system (from render pipeline)
    render::ParticleSystem* particle_system = nullptr;

    // Current state
    bool rain_enabled = false;
    bool snow_enabled = false;
    float rain_intensity = 0.0f;
    float snow_intensity = 0.0f;
    float surface_wetness = 0.0f;

    // Fog state
    bool fog_enabled = false;
    float fog_density = 0.0f;
    Vec3 fog_color{0.7f, 0.75f, 0.8f};
    float fog_height = 100.0f;

    // Screen effects
    bool rain_on_screen = false;

    // Lightning flash
    float lightning_flash_time = 0.0f;
    float lightning_flash_intensity = 0.0f;

    // Particle spawn bounds
    Vec3 particle_center{0.0f};
    Vec3 particle_extents{50.0f, 30.0f, 50.0f};

    // Maximum emission rates for particles
    static constexpr float MAX_RAIN_EMISSION_RATE = 5000.0f;  // particles/sec at intensity 1.0
    static constexpr float MAX_SNOW_EMISSION_RATE = 500.0f;   // particles/sec at intensity 1.0
};

WeatherEffects::WeatherEffects() : m_impl(std::make_unique<Impl>()) {}

WeatherEffects::~WeatherEffects() = default;

void WeatherEffects::initialize(scene::World& world, render::RenderPipeline& pipeline,
                                 const WeatherEffectsConfig& config) {
    m_impl->world = &world;
    m_impl->pipeline = &pipeline;
    m_impl->config = config;

    // Create particle emitter entities
    m_impl->rain_emitter = world.create();
    m_impl->snow_emitter = world.create();

    // Create particle emitter configurations
    m_impl->rain_config = create_rain_config(config);
    m_impl->snow_config = create_snow_config(config);

    // Get particle system from render pipeline
    m_impl->particle_system = pipeline.get_particle_system();

    if (m_impl->particle_system) {
        // Create rain emitter runtime
        m_impl->rain_runtime = m_impl->particle_system->create_emitter_runtime(m_impl->rain_config);
        if (m_impl->rain_runtime) {
            // Start paused until we have rain
            m_impl->particle_system->pause(m_impl->rain_runtime);
        }

        // Create snow emitter runtime
        m_impl->snow_runtime = m_impl->particle_system->create_emitter_runtime(m_impl->snow_config);
        if (m_impl->snow_runtime) {
            // Start paused until we have snow
            m_impl->particle_system->pause(m_impl->snow_runtime);
        }

        core::log(core::LogLevel::Info, "[Environment] WeatherEffects particle emitters created");
    } else {
        core::log(core::LogLevel::Warn, "[Environment] WeatherEffects: No particle system available, weather particles disabled");
    }

    m_impl->initialized = true;

    core::log(core::LogLevel::Info, "[Environment] WeatherEffects initialized");
}

void WeatherEffects::update(double dt) {
    if (!m_impl->initialized) return;

    float fdt = static_cast<float>(dt);

    // Auto-sync with WeatherSystem
    if (m_impl->auto_sync) {
        const WeatherParams& params = get_weather_system().get_current_params();

        // Rain/snow
        if (params.precipitation_intensity > 0.0f) {
            if (params.precipitation_is_snow) {
                m_impl->rain_enabled = false;
                m_impl->snow_enabled = true;
                m_impl->snow_intensity = params.precipitation_intensity;
            } else {
                m_impl->rain_enabled = true;
                m_impl->snow_enabled = false;
                m_impl->rain_intensity = params.precipitation_intensity;
            }
        } else {
            m_impl->rain_enabled = false;
            m_impl->snow_enabled = false;
        }

        // Fog
        m_impl->fog_enabled = params.fog_density > 0.05f;
        m_impl->fog_density = params.fog_density;
        m_impl->fog_color = params.fog_color;
        m_impl->fog_height = params.fog_height;

        // Surface wetness
        if (get_weather_system().is_raining()) {
            // Increase wetness during rain
            m_impl->surface_wetness = std::min(1.0f,
                m_impl->surface_wetness + fdt * 0.1f * params.precipitation_intensity);
        } else {
            // Dry over time
            m_impl->surface_wetness = std::max(0.0f,
                m_impl->surface_wetness - fdt * m_impl->config.dry_speed);
        }
    }

    // Update lightning flash
    if (m_impl->lightning_flash_time > 0.0f) {
        m_impl->lightning_flash_time -= fdt;
        if (m_impl->lightning_flash_time < 0.0f) {
            m_impl->lightning_flash_time = 0.0f;
            m_impl->lightning_flash_intensity = 0.0f;
        }
    }

    // Update particle systems
    if (m_impl->particle_system) {
        // Build emitter transform centered on particle bounds
        Mat4 emitter_transform = glm::translate(Mat4{1.0f}, m_impl->particle_center);
        emitter_transform = glm::translate(emitter_transform, Vec3{0.0f, m_impl->particle_extents.y * 0.5f, 0.0f});

        // Update rain particles
        if (m_impl->rain_runtime) {
            if (m_impl->rain_enabled && m_impl->rain_intensity > 0.01f) {
                // Update emission rate based on intensity
                m_impl->rain_config.emission_rate = m_impl->rain_intensity * Impl::MAX_RAIN_EMISSION_RATE;

                // Update emission shape to match bounds
                m_impl->rain_config.emission_shape.size = Vec3{
                    m_impl->particle_extents.x,
                    1.0f,  // Thin spawn layer
                    m_impl->particle_extents.z
                };

                // Resume if paused
                m_impl->particle_system->play(m_impl->rain_runtime);

                // Update emitter
                m_impl->particle_system->update_emitter(
                    m_impl->rain_runtime,
                    m_impl->rain_config,
                    emitter_transform,
                    fdt
                );
            } else {
                // Pause rain when not active
                m_impl->particle_system->pause(m_impl->rain_runtime);
            }
        }

        // Update snow particles
        if (m_impl->snow_runtime) {
            if (m_impl->snow_enabled && m_impl->snow_intensity > 0.01f) {
                // Update emission rate based on intensity
                m_impl->snow_config.emission_rate = m_impl->snow_intensity * Impl::MAX_SNOW_EMISSION_RATE;

                // Update emission shape to match bounds
                m_impl->snow_config.emission_shape.size = Vec3{
                    m_impl->particle_extents.x * 1.2f,  // Wider for snow
                    2.0f,  // Thicker spawn layer
                    m_impl->particle_extents.z * 1.2f
                };

                // Apply wind from weather system if auto-syncing
                if (m_impl->auto_sync) {
                    const WeatherParams& params = get_weather_system().get_current_params();
                    m_impl->snow_config.velocity_variance = Vec3{
                        1.5f * m_impl->config.snow_turbulence + params.wind_speed * 0.2f,
                        0.5f,
                        1.5f * m_impl->config.snow_turbulence + params.wind_speed * 0.2f
                    };
                }

                // Resume if paused
                m_impl->particle_system->play(m_impl->snow_runtime);

                // Update emitter
                m_impl->particle_system->update_emitter(
                    m_impl->snow_runtime,
                    m_impl->snow_config,
                    emitter_transform,
                    fdt
                );
            } else {
                // Pause snow when not active
                m_impl->particle_system->pause(m_impl->snow_runtime);
            }
        }
    }
}

void WeatherEffects::shutdown() {
    // Destroy particle runtimes
    if (m_impl->particle_system) {
        if (m_impl->rain_runtime) {
            m_impl->particle_system->destroy_emitter_runtime(m_impl->rain_runtime);
            m_impl->rain_runtime = nullptr;
        }
        if (m_impl->snow_runtime) {
            m_impl->particle_system->destroy_emitter_runtime(m_impl->snow_runtime);
            m_impl->snow_runtime = nullptr;
        }
        m_impl->particle_system = nullptr;
    }

    // Destroy entities
    if (m_impl->world && m_impl->rain_emitter != scene::NullEntity) {
        m_impl->world->destroy(m_impl->rain_emitter);
    }
    if (m_impl->world && m_impl->snow_emitter != scene::NullEntity) {
        m_impl->world->destroy(m_impl->snow_emitter);
    }
    m_impl->world = nullptr;
    m_impl->pipeline = nullptr;
    m_impl->initialized = false;

    core::log(core::LogLevel::Info, "[Environment] WeatherEffects shutdown");
}

void WeatherEffects::set_config(const WeatherEffectsConfig& config) {
    m_impl->config = config;
}

const WeatherEffectsConfig& WeatherEffects::get_config() const {
    return m_impl->config;
}

void WeatherEffects::set_rain_enabled(bool enabled) {
    m_impl->rain_enabled = enabled;
}

bool WeatherEffects::is_rain_enabled() const {
    return m_impl->rain_enabled;
}

void WeatherEffects::set_rain_intensity(float intensity) {
    m_impl->rain_intensity = std::clamp(intensity, 0.0f, 1.0f);
}

float WeatherEffects::get_rain_intensity() const {
    return m_impl->rain_intensity;
}

void WeatherEffects::set_snow_enabled(bool enabled) {
    m_impl->snow_enabled = enabled;
}

bool WeatherEffects::is_snow_enabled() const {
    return m_impl->snow_enabled;
}

void WeatherEffects::set_snow_intensity(float intensity) {
    m_impl->snow_intensity = std::clamp(intensity, 0.0f, 1.0f);
}

float WeatherEffects::get_snow_intensity() const {
    return m_impl->snow_intensity;
}

void WeatherEffects::set_particle_bounds(const Vec3& center, const Vec3& extents) {
    m_impl->particle_center = center;
    m_impl->particle_extents = extents;
}

void WeatherEffects::set_surface_wetness(float wetness) {
    m_impl->surface_wetness = std::clamp(wetness, 0.0f, 1.0f);
}

float WeatherEffects::get_surface_wetness() const {
    return m_impl->surface_wetness;
}

void WeatherEffects::set_fog_enabled(bool enabled) {
    m_impl->fog_enabled = enabled;
}

bool WeatherEffects::is_fog_enabled() const {
    return m_impl->fog_enabled;
}

void WeatherEffects::set_fog_density(float density) {
    m_impl->fog_density = std::clamp(density, 0.0f, 1.0f);
}

void WeatherEffects::set_fog_color(const Vec3& color) {
    m_impl->fog_color = color;
}

void WeatherEffects::set_fog_height(float height) {
    m_impl->fog_height = height;
}

void WeatherEffects::set_rain_on_screen_enabled(bool enabled) {
    m_impl->rain_on_screen = enabled;
}

bool WeatherEffects::is_rain_on_screen_enabled() const {
    return m_impl->rain_on_screen;
}

void WeatherEffects::trigger_lightning_flash(float intensity) {
    m_impl->lightning_flash_time = m_impl->config.lightning_flash_duration;
    m_impl->lightning_flash_intensity = intensity * m_impl->config.lightning_intensity;
}

void WeatherEffects::trigger_lightning_flash_at(const Vec3& /*position*/, float intensity) {
    // Position could be used to determine flash direction/intensity
    // For now, just trigger a general flash
    trigger_lightning_flash(intensity);
}

scene::Entity WeatherEffects::get_rain_emitter() const {
    return m_impl->rain_emitter;
}

scene::Entity WeatherEffects::get_snow_emitter() const {
    return m_impl->snow_emitter;
}

void WeatherEffects::set_auto_sync(bool enabled) {
    m_impl->auto_sync = enabled;
}

bool WeatherEffects::get_auto_sync() const {
    return m_impl->auto_sync;
}

// Global instance
WeatherEffects& get_weather_effects() {
    static WeatherEffects instance;
    return instance;
}

} // namespace engine::environment
