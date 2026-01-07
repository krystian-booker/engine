#pragma once

#include <engine/scene/entity.hpp>
#include <engine/scene/entity.hpp>
//#include <engine/scene/world.hpp>
#include <engine/core/math.hpp>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <optional>

namespace engine::scene { class World; }

namespace engine::ai {

using engine::core::Vec3;

// ============================================================================
// Perception Sense Types
// ============================================================================

enum class PerceptionSense : uint8_t {
    Sight,      // Visual perception
    Hearing,    // Audio perception
    Damage      // Awareness of damage source
};

// ============================================================================
// Perceived Entity
// ============================================================================

struct PerceivedEntity {
    scene::Entity entity = scene::NullEntity;
    PerceptionSense sense = PerceptionSense::Sight;

    float stimulation = 1.0f;                   // Perception strength (0-1)
    float time_since_sensed = 0.0f;             // Time since last perception
    float time_first_sensed = 0.0f;             // When first perceived (for awareness buildup)

    Vec3 last_known_position{0.0f};
    Vec3 last_known_velocity{0.0f};

    bool currently_perceived = false;           // Actively perceived right now
    bool is_hostile = false;                    // Friend or foe

    // Awareness level (builds up over time when perceiving)
    float awareness = 0.0f;                     // 0 = unaware, 1 = fully aware
};

// ============================================================================
// AI Perception Component
// ============================================================================

struct AIPerceptionComponent {
    bool enabled = true;

    // ========================================================================
    // Sight Configuration
    // ========================================================================
    bool sight_enabled = true;
    float sight_range = 20.0f;                  // Maximum sight distance
    float sight_angle = 120.0f;                 // Field of view (degrees)
    float sight_height_tolerance = 5.0f;        // Vertical tolerance
    uint32_t sight_layer_mask = 0xFFFFFFFF;     // Physics layers for LOS check
    bool requires_line_of_sight = true;         // Raycast check

    // Peripheral vision (wider angle but lower stimulation)
    bool peripheral_enabled = true;
    float peripheral_angle = 180.0f;            // Peripheral FOV
    float peripheral_stimulation = 0.3f;        // Reduced awareness in peripheral

    // ========================================================================
    // Hearing Configuration
    // ========================================================================
    bool hearing_enabled = true;
    float hearing_range = 15.0f;                // Maximum hearing distance
    float hearing_through_walls = 0.3f;         // Hearing multiplier through walls

    // ========================================================================
    // Awareness Configuration
    // ========================================================================
    float awareness_gain_rate = 2.0f;           // How fast awareness builds (per second)
    float awareness_decay_rate = 0.5f;          // How fast awareness decays when not perceiving
    float awareness_threshold = 0.8f;           // Awareness level to become "alert"
    float instant_awareness_distance = 3.0f;    // Instant full awareness within this range

    // ========================================================================
    // Memory
    // ========================================================================
    float memory_duration = 10.0f;              // How long to remember after losing perception
    float position_prediction_time = 1.0f;      // How far to predict movement

    // ========================================================================
    // Faction
    // ========================================================================
    std::string faction = "enemy";
    std::vector<std::string> hostile_factions = {"player"};
    std::vector<std::string> friendly_factions;

    // ========================================================================
    // Current Perceptions
    // ========================================================================
    std::vector<PerceivedEntity> perceived_entities;

    // ========================================================================
    // Helpers
    // ========================================================================

    // Get the most threatening perceived entity
    scene::Entity get_primary_threat() const {
        float highest_threat = 0.0f;
        scene::Entity result = scene::NullEntity;

        for (const auto& pe : perceived_entities) {
            if (!pe.is_hostile) continue;
            float threat = pe.awareness * pe.stimulation;
            if (pe.currently_perceived) threat *= 2.0f;

            if (threat > highest_threat) {
                highest_threat = threat;
                result = pe.entity;
            }
        }
        return result;
    }

    // Get nearest perceived hostile
    scene::Entity get_nearest_threat(const Vec3& position) const {
        float min_dist = std::numeric_limits<float>::max();
        scene::Entity result = scene::NullEntity;

        for (const auto& pe : perceived_entities) {
            if (!pe.is_hostile || pe.awareness < awareness_threshold) continue;
            float dist = glm::length(pe.last_known_position - position);
            if (dist < min_dist) {
                min_dist = dist;
                result = pe.entity;
            }
        }
        return result;
    }

    // Check if entity is currently perceived
    bool can_see(scene::Entity e) const {
        for (const auto& pe : perceived_entities) {
            if (pe.entity == e && pe.currently_perceived &&
                pe.sense == PerceptionSense::Sight) {
                return true;
            }
        }
        return false;
    }

    // Check if entity is in memory
    bool is_aware_of(scene::Entity e) const {
        for (const auto& pe : perceived_entities) {
            if (pe.entity == e && pe.awareness >= awareness_threshold) {
                return true;
            }
        }
        return false;
    }

    // Get last known position of entity
    std::optional<Vec3> get_last_known_position(scene::Entity e) const {
        for (const auto& pe : perceived_entities) {
            if (pe.entity == e) {
                return pe.last_known_position;
            }
        }
        return std::nullopt;
    }

    // Get predicted position (last known + velocity * time)
    Vec3 get_predicted_position(scene::Entity e, float prediction_time) const {
        for (const auto& pe : perceived_entities) {
            if (pe.entity == e) {
                return pe.last_known_position + pe.last_known_velocity * prediction_time;
            }
        }
        return Vec3(0.0f);
    }

    // Check if any hostile is perceived
    bool has_threat() const {
        for (const auto& pe : perceived_entities) {
            if (pe.is_hostile && pe.awareness >= awareness_threshold) {
                return true;
            }
        }
        return false;
    }

    // Get awareness level of specific entity
    float get_awareness_of(scene::Entity e) const {
        for (const auto& pe : perceived_entities) {
            if (pe.entity == e) {
                return pe.awareness;
            }
        }
        return 0.0f;
    }
};

// ============================================================================
// Noise Emitter Component
// ============================================================================

struct AINoiseEmitterComponent {
    bool enabled = true;
    float noise_radius = 5.0f;                  // How far the noise travels
    float loudness = 1.0f;                      // Multiplier for perception

    bool is_continuous = false;                 // Continuous vs one-shot
    std::string noise_type = "generic";         // For filtering (footsteps, gunshot, etc.)

    // For one-shot noises
    bool trigger_noise = false;                 // Set to true to emit noise once
    float last_noise_time = 0.0f;               // When last noise was emitted

    // Position override (if empty, uses entity position)
    std::optional<Vec3> noise_position;
};

// ============================================================================
// Perception Events
// ============================================================================

struct PerceptionGainedEvent {
    scene::Entity perceiver;
    scene::Entity perceived;
    PerceptionSense sense;
};

struct PerceptionLostEvent {
    scene::Entity perceiver;
    scene::Entity perceived;
};

struct AwarenessChangedEvent {
    scene::Entity perceiver;
    scene::Entity perceived;
    float old_awareness;
    float new_awareness;
    bool became_alert;                          // Crossed awareness threshold
};

struct NoiseHeardEvent {
    scene::Entity listener;
    Vec3 noise_position;
    float loudness;
    std::string noise_type;
    scene::Entity noise_source;                 // May be NullEntity for environmental noise
};

struct AIAlertedEvent {
    scene::Entity entity;
    scene::Entity cause;
    Vec3 alert_position;
};

// ============================================================================
// Perception System
// ============================================================================

// Line of sight check function
using PerceptionLOSCheck = std::function<bool(scene::World&, const Vec3&, const Vec3&,
                                               uint32_t layer_mask, scene::Entity exclude)>;

class PerceptionSystem {
public:
    static PerceptionSystem& instance();

    // Delete copy/move
    PerceptionSystem(const PerceptionSystem&) = delete;
    PerceptionSystem& operator=(const PerceptionSystem&) = delete;

    // Update perception for all AI entities
    void update(scene::World& world, float dt);

    // Force check perception for a specific entity
    void check_perception(scene::World& world, scene::Entity entity, float dt);

    // Emit a noise at a position
    void emit_noise(scene::World& world, const Vec3& position, float radius,
                    float loudness = 1.0f, const std::string& type = "generic",
                    scene::Entity source = scene::NullEntity);

    // Instantly alert an AI to a target
    void alert_to_target(scene::World& world, scene::Entity ai, scene::Entity target);

    // Check if one entity can see another
    bool can_see(scene::World& world, scene::Entity observer, scene::Entity target);

    // Get distance to target
    float get_distance(scene::World& world, scene::Entity from, scene::Entity to);

    // Configuration
    void set_los_check(PerceptionLOSCheck check);
    void set_global_sight_multiplier(float mult) { m_sight_multiplier = mult; }
    void set_global_hearing_multiplier(float mult) { m_hearing_multiplier = mult; }

private:
    PerceptionSystem();
    ~PerceptionSystem() = default;

    void update_sight(scene::World& world, scene::Entity entity,
                      AIPerceptionComponent& perception, const Vec3& position,
                      const Vec3& forward, float dt);

    void update_hearing(scene::World& world, scene::Entity entity,
                        AIPerceptionComponent& perception, const Vec3& position, float dt);

    void update_awareness(AIPerceptionComponent& perception, float dt);
    void cleanup_perceptions(AIPerceptionComponent& perception, float dt);

    bool default_los_check(scene::World& world, const Vec3& from, const Vec3& to,
                           uint32_t layer_mask, scene::Entity exclude);

    bool is_in_fov(const Vec3& observer_pos, const Vec3& forward,
                   const Vec3& target_pos, float angle, float range);

    PerceptionLOSCheck m_los_check;
    float m_sight_multiplier = 1.0f;
    float m_hearing_multiplier = 1.0f;
};

// Convenience accessor
inline PerceptionSystem& perception() {
    return PerceptionSystem::instance();
}

// ============================================================================
// ECS Systems
// ============================================================================

// Main perception update system (FixedUpdate phase)
void perception_system(scene::World& world, double dt);

// Process noise emitters (FixedUpdate phase)
void noise_emitter_system(scene::World& world, double dt);

// ============================================================================
// Registration
// ============================================================================

void register_perception_components();

} // namespace engine::ai
