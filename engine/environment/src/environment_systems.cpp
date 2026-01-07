#include <engine/environment/environment_components.hpp>
#include <engine/environment/weather.hpp>
#include <engine/environment/weather_audio.hpp>
#include <engine/environment/time_of_day.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <engine/scene/systems.hpp>
#include <engine/core/log.hpp>

namespace engine::environment {

// System: Update WeatherZone components
void weather_zone_system(scene::World& world, double /*dt*/) {
    // Get player/camera position for zone detection
    // In a full implementation, you'd get this from the active camera or player entity

    auto view = world.view<WeatherZone, scene::WorldTransform>();
    for (auto entity : view) {
        auto& zone = view.get<WeatherZone>(entity);
        auto& transform = view.get<scene::WorldTransform>(entity);

        if (!zone.enabled) continue;

        // Check if player is inside this zone
        // For now, we just mark zones as available for the weather system to query

        (void)transform;  // Unused for now
    }
}

// System: Update IndoorVolume components
void indoor_volume_system(scene::World& world, double /*dt*/) {
    bool any_indoor = false;

    auto view = world.view<IndoorVolume, scene::WorldTransform>();
    for (auto entity : view) {
        auto& volume = view.get<IndoorVolume>(entity);
        auto& transform = view.get<scene::WorldTransform>(entity);

        if (!volume.enabled) continue;

        // Check if player is inside this volume
        // In a full implementation, you'd do AABB or sphere overlap tests

        (void)transform;  // Unused for now

        // If player is inside any indoor volume, set indoor state
        // For demonstration, we'll just track if any exist
    }

    // Update weather audio indoor state
    get_weather_audio().set_indoor(any_indoor);
}

// System: Update TimeOfDayListener components
void time_listener_system(scene::World& world, double /*dt*/) {
    float current_hour = get_time_of_day().get_time();
    TimePeriod current_period = get_time_of_day().get_current_period();

    auto view = world.view<TimeOfDayListener>();
    for (auto entity : view) {
        auto& listener = view.get<TimeOfDayListener>(entity);

        if (!listener.enabled) continue;

        // Fire update callback
        if (listener.on_update) {
            listener.on_update(current_hour);
        }

        // Check hour triggers
        for (auto& trigger : listener.hour_triggers) {
            if (trigger.triggered_today) continue;

            // Simple hour comparison (could be improved with proper time delta checking)
            if (std::abs(current_hour - trigger.hour) < 0.05f) {
                if (trigger.callback) {
                    trigger.callback();
                }
                trigger.triggered_today = true;
            }
        }

        (void)current_period;  // Period change handled via global callback
    }
}

// System: Update WeatherReactive components
void weather_reactive_system(scene::World& world, double dt) {
    float fdt = static_cast<float>(dt);
    const WeatherParams& weather = get_weather_system().get_current_params();

    auto view = world.view<WeatherReactive>();
    for (auto entity : view) {
        auto& reactive = view.get<WeatherReactive>(entity);

        // Update wetness
        if (reactive.affected_by_wetness) {
            if (get_weather_system().is_raining()) {
                // Get wetter during rain
                reactive.current_wetness = std::min(1.0f,
                    reactive.current_wetness + fdt * 0.1f * weather.precipitation_intensity);
            } else {
                // Dry over time
                reactive.current_wetness = std::max(0.0f,
                    reactive.current_wetness - fdt * 0.05f);
            }
        }

        // Update snow accumulation
        if (reactive.can_accumulate_snow) {
            if (get_weather_system().is_snowing()) {
                reactive.current_snow = std::min(1.0f,
                    reactive.current_snow + fdt * reactive.snow_accumulation_rate * weather.precipitation_intensity);
            } else if (weather.temperature > 0.0f) {
                // Melt snow
                reactive.current_snow = std::max(0.0f,
                    reactive.current_snow - fdt * reactive.snow_melt_rate);
            }
        }

        // In a full implementation, you'd update material parameters here
        // via the render system or material instance
    }
}

// System: Update WindAffected components
void wind_affected_system(scene::World& world, double dt) {
    float fdt = static_cast<float>(dt);
    const WeatherParams& weather = get_weather_system().get_current_params();

    Vec3 wind = weather.wind_direction * weather.wind_speed;

    auto view = world.view<WindAffected>();
    for (auto entity : view) {
        auto& affected = view.get<WindAffected>(entity);

        if (!affected.enabled) continue;

        // Calculate wind effect with inertia
        Vec3 target_wind = (wind + affected.local_wind_offset) * affected.wind_strength_multiplier;

        // Smooth interpolation based on inertia
        float blend = 1.0f - std::exp(-fdt / affected.inertia);
        affected.current_wind_effect = glm::mix(affected.current_wind_effect, target_wind, blend);

        // Add oscillation
        static float time = 0.0f;
        time += fdt * affected.oscillation_frequency;
        Vec3 oscillation = Vec3(
            std::sin(time * 1.0f),
            std::sin(time * 0.7f + 1.0f),
            std::sin(time * 1.3f + 2.0f)
        ) * affected.oscillation_amplitude * weather.wind_gustiness;

        affected.current_wind_effect += oscillation;

        // In a full implementation, you'd apply this to transform or skeleton
    }
}

// System: Update LightningAttractor components
void lightning_attractor_system(scene::World& world, double dt) {
    float fdt = static_cast<float>(dt);

    auto view = world.view<LightningAttractor, scene::WorldTransform>();
    for (auto entity : view) {
        auto& attractor = view.get<LightningAttractor>(entity);
        // auto& transform = view.get<scene::WorldTransform>(entity);

        // Update cooldown timer
        attractor.time_since_last_strike += fdt;
    }

    // Lightning strike selection would be done in WeatherSystem's thunder callback
    // It would query all LightningAttractor components and pick one based on
    // attraction strength and distance
}

// System: Update EnvironmentProbe components
void environment_probe_system(scene::World& world, double dt) {
    float fdt = static_cast<float>(dt);
    const WeatherParams& weather = get_weather_system().get_current_params();

    auto view = world.view<EnvironmentProbe, scene::WorldTransform>();
    for (auto entity : view) {
        auto& probe = view.get<EnvironmentProbe>(entity);
        // auto& transform = view.get<scene::WorldTransform>(entity);

        if (!probe.enabled) continue;

        probe.time_since_update += fdt;
        if (probe.time_since_update < probe.update_interval) continue;

        probe.time_since_update = 0.0f;

        // Update probe values from global weather
        probe.temperature = weather.temperature;
        probe.wetness = weather.wetness;
        probe.wind_speed = weather.wind_speed;
        probe.wind_direction = weather.wind_direction;
        probe.light_intensity = get_time_of_day().get_sun_intensity();

        // Indoor check would use spatial queries against IndoorVolume components
        probe.is_indoor = false;
    }
}

// Register all environment systems with the scene scheduler
void register_environment_systems(scene::Scheduler& scheduler) {
    scheduler.add(scene::Phase::Update, weather_zone_system, "weather_zone", 0);
    scheduler.add(scene::Phase::Update, indoor_volume_system, "indoor_volume", 0);
    scheduler.add(scene::Phase::Update, time_listener_system, "time_listener", 0);
    scheduler.add(scene::Phase::Update, weather_reactive_system, "weather_reactive", 0);
    scheduler.add(scene::Phase::Update, wind_affected_system, "wind_affected", 0);
    scheduler.add(scene::Phase::Update, lightning_attractor_system, "lightning_attractor", 0);
    scheduler.add(scene::Phase::Update, environment_probe_system, "environment_probe", 0);
}

} // namespace engine::environment
