#include <engine/audio/audio_system.hpp>
#include <engine/audio/audio_engine.hpp>
#include <engine/audio/audio_components.hpp>
#include <engine/scene/transform.hpp>
#include <algorithm>

namespace engine::audio {

using namespace engine::scene;
using namespace engine::core;

// Static member initialization
Vec3 AudioSystem::s_listener_position{0.0f};
Vec3 AudioSystem::s_listener_forward{0.0f, 0.0f, -1.0f};

void AudioSystem::init(World& /*world*/) {
    // Nothing to initialize - AudioEngine is already a singleton
}

void AudioSystem::shutdown() {
    // Nothing to cleanup
}

void AudioSystem::update_listener(World& world, double /*dt*/) {
    auto& audio = get_audio_engine();

    // Find the highest-priority active listener
    AudioListener* best_listener = nullptr;
    LocalTransform* best_local = nullptr;
    WorldTransform* best_world = nullptr;

    auto view = world.view<AudioListener, LocalTransform>();
    for (auto entity : view) {
        auto& listener = view.get<AudioListener>(entity);
        if (!listener.active) continue;

        if (!best_listener || listener.priority > best_listener->priority) {
            best_listener = &listener;
            best_local = &view.get<LocalTransform>(entity);

            // Try to get world transform if available
            auto* wt = world.try_get<WorldTransform>(entity);
            best_world = wt;
        }
    }

    if (best_listener && best_local) {
        // Use world transform if available, otherwise use local
        Vec3 position;
        Vec3 forward;
        Vec3 up;

        if (best_world) {
            position = best_world->position();
            Quat rot = best_world->rotation();
            forward = rot * Vec3{0.0f, 0.0f, -1.0f};
            up = rot * Vec3{0.0f, 1.0f, 0.0f};
        } else {
            position = best_local->position;
            forward = best_local->forward();
            up = best_local->up();
        }

        // Update engine listener
        audio.set_listener_position(position);
        audio.set_listener_orientation(forward, up);
        audio.set_listener_velocity(best_listener->velocity);

        // Cache for other systems
        s_listener_position = position;
        s_listener_forward = forward;
    }
}

void AudioSystem::update_sources(World& world, double /*dt*/) {
    auto& audio = get_audio_engine();

    auto view = world.view<AudioSource, LocalTransform>();
    for (auto entity : view) {
        auto& source = view.get<AudioSource>(entity);
        auto& local = view.get<LocalTransform>(entity);

        if (!source.sound.valid()) continue;

        // Get world position
        Vec3 position = local.position;
        auto* wt = world.try_get<WorldTransform>(entity);
        if (wt) {
            position = wt->position();
        }

        bool currently_playing = audio.is_sound_playing(source.sound);

        // Handle play state changes
        if (source.playing && !currently_playing) {
            // Start playing
            SoundConfig config;
            config.volume = source.volume;
            config.pitch = source.pitch;
            config.loop = source.loop;
            config.spatial = source.spatial;

            if (source.spatial) {
                audio.play_sound_3d(source.sound, position, config);
            } else {
                audio.play_sound(source.sound, config);
            }
        } else if (!source.playing && currently_playing) {
            // Stop playing
            audio.stop_sound(source.sound);
        } else if (source.playing && currently_playing && source.spatial) {
            // Update position for moving sources
            audio.set_sound_position(source.sound, position);
        }

        // Compute attenuation for visualization/debugging
        if (source.spatial) {
            float distance = glm::distance(position, s_listener_position);
            source.computed_volume = calculate_attenuation(
                distance,
                source.min_distance,
                source.max_distance,
                source.attenuation,
                source.rolloff
            );

            // Apply cone attenuation if enabled
            if (source.use_cone) {
                Vec3 source_forward = local.forward();
                if (wt) {
                    source_forward = wt->rotation() * Vec3{0.0f, 0.0f, -1.0f};
                }
                Vec3 to_listener = s_listener_position - position;
                float cone_atten = calculate_cone_attenuation(
                    source_forward,
                    to_listener,
                    source.cone_inner_angle,
                    source.cone_outer_angle,
                    source.cone_outer_volume
                );
                source.computed_volume *= cone_atten;
            }
        }
    }
}

void AudioSystem::process_triggers(World& world, double dt) {
    auto& audio = get_audio_engine();

    auto view = world.view<AudioTrigger, LocalTransform>();
    for (auto entity : view) {
        auto& trigger = view.get<AudioTrigger>(entity);
        auto& local = view.get<LocalTransform>(entity);

        if (!trigger.sound.valid()) continue;

        // Get world position
        Vec3 position = local.position;
        auto* wt = world.try_get<WorldTransform>(entity);
        if (wt) {
            position = wt->position();
        }

        // Update cooldown
        trigger.cooldown_timer = std::max(0.0f, trigger.cooldown_timer - static_cast<float>(dt));

        // Check distance to listener
        float distance = glm::distance(position, s_listener_position);
        bool in_range = distance <= trigger.trigger_radius;

        if (in_range && !trigger.triggered && trigger.cooldown_timer <= 0.0f) {
            // Trigger the sound
            audio.play_sound_3d(trigger.sound, position, {});
            trigger.triggered = true;

            if (!trigger.one_shot) {
                trigger.cooldown_timer = trigger.cooldown;
            }
        } else if (!in_range) {
            // Reset trigger when leaving range
            trigger.triggered = false;
        }
    }
}

void AudioSystem::update_reverb_zones(World& world, double /*dt*/) {
    // Find the closest/strongest reverb zone affecting the listener
    float best_blend = 0.0f;
    ReverbZone* active_zone = nullptr;

    auto view = world.view<ReverbZone, LocalTransform>();
    for (auto entity : view) {
        auto& zone = view.get<ReverbZone>(entity);
        auto& local = view.get<LocalTransform>(entity);

        if (!zone.active) continue;

        // Get world position
        Vec3 position = local.position;
        auto* wt = world.try_get<WorldTransform>(entity);
        if (wt) {
            position = wt->position();
        }

        float distance = glm::distance(position, s_listener_position);

        if (distance < zone.max_distance) {
            float blend;
            if (distance <= zone.min_distance) {
                blend = 1.0f;  // Full reverb
            } else {
                blend = 1.0f - (distance - zone.min_distance) / (zone.max_distance - zone.min_distance);
            }

            if (blend > best_blend) {
                best_blend = blend;
                active_zone = &zone;
            }
        }
    }

    // TODO: Apply reverb parameters to audio engine when effects system is implemented
    // For now, we just identify which zone is active
    (void)active_zone;
}

void AudioSystem::register_systems(Scheduler& scheduler) {
    // PreUpdate: Update listener position (before other audio processing)
    scheduler.add(Phase::PreUpdate, update_listener, "audio_listener", 0);

    // Update: Process audio sources and triggers
    scheduler.add(Phase::Update, update_sources, "audio_sources", 0);
    scheduler.add(Phase::Update, process_triggers, "audio_triggers", 1);

    // PostUpdate: Update reverb zones
    scheduler.add(Phase::PostUpdate, update_reverb_zones, "audio_reverb", 0);
}

} // namespace engine::audio
