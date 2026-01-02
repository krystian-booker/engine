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

} // namespace engine::cinematic
