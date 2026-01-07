#pragma once

#include <engine/scene/entity.hpp>
#include <engine/core/math.hpp>
#include <string>
#include <vector>
#include <cstdint>

namespace engine::combat {

using engine::core::Vec3;

// Collision shape for hitboxes and hurtboxes
enum class CollisionShape : uint8_t {
    Sphere,
    Box,
    Capsule
};

// Hitbox component - active damage dealer
struct HitboxComponent {
    bool active = false;                        // Only checks collisions when active
    std::string hitbox_id;                      // Identifier for callbacks

    // Shape definition
    CollisionShape shape = CollisionShape::Sphere;
    Vec3 center_offset{0.0f, 0.0f, 0.0f};       // Local space offset from entity
    Vec3 half_extents{0.5f, 0.5f, 0.5f};        // For Box shape
    float radius = 0.5f;                        // For Sphere/Capsule
    float height = 1.0f;                        // For Capsule

    // Damage configuration
    float base_damage = 10.0f;
    std::string damage_type = "physical";       // Physical, fire, ice, lightning, etc.
    float knockback_force = 5.0f;
    Vec3 knockback_direction{0.0f, 0.0f, 1.0f}; // Local space direction

    // Poise/stagger
    float poise_damage = 10.0f;                 // Stagger buildup
    bool causes_stagger = true;

    // Critical hit support
    float critical_multiplier = 1.5f;
    float critical_chance = 0.0f;               // 0.0 to 1.0

    // Hit registration
    std::vector<scene::Entity> already_hit;     // Entities hit this activation
    int max_hits = -1;                          // -1 = unlimited
    float hit_cooldown_per_target = 0.5f;       // Minimum time between hits on same target

    // Faction filtering
    std::string faction = "player";
    std::vector<std::string> target_factions = {"enemy"};

    // Audio/Visual feedback
    std::string hit_sound;                      // Sound to play on hit
    std::string hit_effect;                     // Particle effect on hit

    // Helper to check if entity was already hit
    bool was_hit(scene::Entity entity) const {
        for (auto e : already_hit) {
            if (e == entity) return true;
        }
        return false;
    }

    // Clear hit list (call when deactivating or starting new attack)
    void clear_hit_list() {
        already_hit.clear();
    }

    // Activate the hitbox
    void activate() {
        active = true;
        clear_hit_list();
    }

    // Deactivate the hitbox
    void deactivate() {
        active = false;
    }
};

// Result of a hitbox overlap test
struct HitboxOverlap {
    scene::Entity attacker = scene::NullEntity;
    scene::Entity target = scene::NullEntity;
    Vec3 hit_point;                             // World space contact point
    Vec3 hit_normal;                            // Surface normal at hit
    std::string hitbox_id;
    std::string hurtbox_type;
};

} // namespace engine::combat
