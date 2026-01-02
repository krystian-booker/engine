#pragma once

#include <engine/scene/world.hpp>
#include <engine/scene/systems.hpp>
#include <engine/audio/audio_components.hpp>
#include <engine/core/math.hpp>

namespace engine::audio {

using namespace engine::scene;
using namespace engine::core;

// Audio system manages audio playback for entities with audio components.
// Processes AudioSource, AudioListener, AudioTrigger, and ReverbZone components.
class AudioSystem {
public:
    AudioSystem() = default;
    ~AudioSystem() = default;

    // Initialize the audio system
    void init(World& world);

    // Shutdown the audio system
    void shutdown();

    // System functions registered with Scheduler by Application::register_engine_systems()
    // All audio systems run in PostUpdate phase, after transform_system, to ensure
    // WorldTransform data is fresh for accurate 3D audio positioning.

    // Update the audio listener position (PostUpdate, priority 5)
    // Finds the highest-priority active AudioListener and updates the AudioEngine
    static void update_listener(World& world, double dt);

    // Update audio sources (PostUpdate, priority 4)
    // Syncs AudioSource component state with actual audio playback
    static void update_sources(World& world, double dt);

    // Process audio triggers (PostUpdate, priority 3)
    // Plays sounds when entities enter AudioTrigger zones
    static void process_triggers(World& world, double dt);

    // Update reverb zones (PostUpdate, priority 2)
    // Applies reverb effects based on listener proximity to ReverbZone entities
    static void update_reverb_zones(World& world, double dt);

    // Register all audio systems with a Scheduler
    // DEPRECATED: Audio systems are now auto-registered by Application::register_engine_systems()
    // This method is kept for backward compatibility with custom schedulers.
    [[deprecated("Audio systems are now auto-registered by Application")]]
    static void register_systems(Scheduler& scheduler);

private:
    // Cached listener position for trigger/reverb calculations
    static Vec3 s_listener_position;
    static Vec3 s_listener_forward;
};

} // namespace engine::audio
