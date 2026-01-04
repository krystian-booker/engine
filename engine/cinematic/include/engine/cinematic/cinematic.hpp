#pragma once

// Umbrella header for engine::cinematic module

#include <engine/cinematic/track.hpp>
#include <engine/cinematic/camera_track.hpp>
#include <engine/cinematic/animation_track.hpp>
#include <engine/cinematic/audio_track.hpp>
#include <engine/cinematic/event_track.hpp>
#include <engine/cinematic/light_track.hpp>
#include <engine/cinematic/postprocess_track.hpp>
#include <engine/cinematic/sequence.hpp>
#include <engine/cinematic/player.hpp>

namespace engine::cinematic {

// Module version
constexpr int VERSION_MAJOR = 1;
constexpr int VERSION_MINOR = 0;

// ECS system function - updates all active cinematic players
// Registered in Application::register_engine_systems() at Update phase
void cinematic_update_system(scene::World& world, double dt);

} // namespace engine::cinematic
