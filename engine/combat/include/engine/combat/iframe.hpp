#pragma once

#include <engine/scene/entity.hpp>
#include <engine/scene/world.hpp>
#include <engine/core/math.hpp>
#include <cstdint>

namespace engine::combat {

using engine::core::Vec3;

// Invincibility frame sources
enum class IFrameSource : uint8_t {
    Dodge,          // Rolling/dodging
    Hit,            // Brief immunity after taking damage
    Attack,         // Some attacks grant i-frames (e.g., counter moves)
    Skill,          // Ability-granted invincibility
    Spawn,          // Brief immunity on spawn
    Custom          // Game-defined source
};

// Invincibility frames component
struct IFrameComponent {
    bool is_invincible = false;                 // Currently invincible
    float remaining_time = 0.0f;                // Time left in i-frames
    IFrameSource source = IFrameSource::Dodge;  // What caused the i-frames

    // Visual feedback during i-frames
    bool flash_enabled = true;                  // Enable flash effect
    float flash_interval = 0.1f;                // Time between flashes
    float flash_timer = 0.0f;                   // Current flash state
    bool flash_visible = true;                  // Current visibility in flash cycle
    Vec3 flash_color{1.0f, 1.0f, 1.0f};        // Tint during flash

    // Audio feedback
    std::string dodge_sound;                    // Sound when i-frames start

    // Default durations by source
    static constexpr float DEFAULT_DODGE_DURATION = 0.4f;
    static constexpr float DEFAULT_HIT_DURATION = 0.5f;
    static constexpr float DEFAULT_SPAWN_DURATION = 2.0f;

    // Grant i-frames
    void grant(float duration, IFrameSource src = IFrameSource::Custom) {
        // Only extend if new duration is longer
        if (duration > remaining_time) {
            remaining_time = duration;
            source = src;
        }
        is_invincible = true;
        flash_timer = 0.0f;
        flash_visible = true;
    }

    // Grant with default duration for source
    void grant_default(IFrameSource src) {
        float duration = 0.3f;
        switch (src) {
            case IFrameSource::Dodge: duration = DEFAULT_DODGE_DURATION; break;
            case IFrameSource::Hit: duration = DEFAULT_HIT_DURATION; break;
            case IFrameSource::Spawn: duration = DEFAULT_SPAWN_DURATION; break;
            default: break;
        }
        grant(duration, src);
    }

    // Update i-frames (returns true when i-frames end)
    bool update(float dt) {
        if (!is_invincible) return false;

        remaining_time -= dt;

        // Update flash effect
        if (flash_enabled) {
            flash_timer += dt;
            if (flash_timer >= flash_interval) {
                flash_timer -= flash_interval;
                flash_visible = !flash_visible;
            }
        }

        // Check if i-frames ended
        if (remaining_time <= 0.0f) {
            remaining_time = 0.0f;
            is_invincible = false;
            flash_visible = true;
            return true; // I-frames ended
        }

        return false;
    }

    // Cancel i-frames early
    void cancel() {
        is_invincible = false;
        remaining_time = 0.0f;
        flash_visible = true;
    }

    // Get progress through i-frames (1.0 = just started, 0.0 = ending)
    float get_progress() const {
        if (!is_invincible) return 0.0f;
        // This requires knowing original duration, so we estimate
        return remaining_time > 0.0f ? 1.0f : 0.0f;
    }
};

// Utility functions for i-frame management
namespace iframe {

// Grant i-frames to an entity
void grant(scene::World& world, scene::Entity entity, float duration,
           IFrameSource source = IFrameSource::Custom);

// Grant default i-frames for a source type
void grant_default(scene::World& world, scene::Entity entity, IFrameSource source);

// Check if entity is currently invincible
bool is_invincible(scene::World& world, scene::Entity entity);

// Cancel i-frames on entity
void cancel(scene::World& world, scene::Entity entity);

// Get remaining i-frame time
float get_remaining(scene::World& world, scene::Entity entity);

} // namespace iframe

} // namespace engine::combat
