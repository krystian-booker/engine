#pragma once

// Umbrella header for engine::environment module
#include <engine/environment/time_of_day.hpp>
#include <engine/environment/sky_controller.hpp>
#include <engine/environment/environment_lighting.hpp>
#include <engine/environment/weather.hpp>
#include <engine/environment/weather_effects.hpp>
#include <engine/environment/weather_audio.hpp>
#include <engine/environment/environment_components.hpp>

// Forward declarations
namespace engine::scene {
    class Scheduler;
}

namespace engine::environment {

// Register all environment ECS systems with the scheduler
void register_environment_systems(scene::Scheduler& scheduler);

// Register environment types with the reflection system
void register_environment_types();

} // namespace engine::environment
