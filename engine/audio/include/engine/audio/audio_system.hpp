#pragma once

#include <engine/scene/world.hpp>
#include <engine/scene/systems.hpp>

namespace engine::audio {

using namespace engine::scene;

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

    // System functions that can be registered with Scheduler
    // These process entities with audio components each frame

    // Update the audio listener position (runs in PreUpdate)
    // Finds the highest-priority active AudioListener and updates the AudioEngine
    static void update_listener(World& world, double dt);

    // Update audio sources (runs in Update)
    // Syncs AudioSource component state with actual audio playback
    static void update_sources(World& world, double dt);

    // Process audio triggers (runs in Update)
    // Plays sounds when entities enter AudioTrigger zones
    static void process_triggers(World& world, double dt);

    // Update reverb zones (runs in PostUpdate)
    // Applies reverb effects based on listener proximity to ReverbZone entities
    static void update_reverb_zones(World& world, double dt);

    // Register all audio systems with a Scheduler
    static void register_systems(Scheduler& scheduler);

private:
    // Cached listener position for trigger/reverb calculations
    static Vec3 s_listener_position;
    static Vec3 s_listener_forward;
};

} // namespace engine::audio
