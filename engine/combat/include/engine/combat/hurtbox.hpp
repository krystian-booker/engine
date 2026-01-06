#pragma once

#include <engine/combat/hitbox.hpp>
#include <engine/scene/entity.hpp>
#include <engine/core/math.hpp>
#include <string>

namespace engine::combat {

// Hurtbox component - receives damage
struct HurtboxComponent {
    bool enabled = true;                        // Can receive damage when enabled

    // Shape definition (same as hitbox)
    CollisionShape shape = CollisionShape::Sphere;
    Vec3 center_offset{0.0f, 0.0f, 0.0f};       // Local space offset from entity
    Vec3 half_extents{0.5f, 0.5f, 0.5f};        // For Box shape
    float radius = 0.5f;                        // For Sphere/Capsule
    float height = 1.0f;                        // For Capsule

    // Body part type for damage multipliers
    std::string hurtbox_type = "body";          // Head, body, limb, weakpoint, armor

    // Damage modifiers
    float damage_multiplier = 1.0f;             // 1.0 = normal, 2.0 = weakpoint, 0.5 = armored
    float poise_multiplier = 1.0f;              // Stagger resistance modifier

    // Type-specific resistances (reduce damage of specific types)
    float physical_resistance = 0.0f;           // 0.0 to 1.0 (percentage reduction)
    float fire_resistance = 0.0f;
    float ice_resistance = 0.0f;
    float lightning_resistance = 0.0f;

    // Faction for filtering
    std::string faction = "enemy";

    // Get resistance for a damage type
    float get_resistance(const std::string& damage_type) const {
        if (damage_type == "physical") return physical_resistance;
        if (damage_type == "fire") return fire_resistance;
        if (damage_type == "ice") return ice_resistance;
        if (damage_type == "lightning") return lightning_resistance;
        return 0.0f;
    }
};

// Component to track entity's total damage received (for health systems)
struct DamageReceiverComponent {
    bool can_receive_damage = true;             // Master toggle

    // Total poise/stagger tracking
    float max_poise = 100.0f;
    float current_poise = 100.0f;
    float poise_recovery_rate = 20.0f;          // Per second
    float poise_recovery_delay = 2.0f;          // Seconds after hit before recovery
    float time_since_hit = 0.0f;

    // Block/parry state (set by game logic)
    bool is_blocking = false;
    bool is_parrying = false;
    float block_damage_reduction = 0.5f;        // 50% damage when blocking
    float parry_window = 0.0f;                  // Remaining parry frames

    // Direction checking for backstabs
    bool backstab_vulnerable = true;
    float backstab_multiplier = 2.0f;
    float backstab_angle_threshold = 60.0f;     // Degrees from behind

    // Apply poise damage, returns true if staggered
    bool apply_poise_damage(float amount) {
        current_poise -= amount;
        time_since_hit = 0.0f;
        if (current_poise <= 0.0f) {
            current_poise = 0.0f;
            return true; // Staggered
        }
        return false;
    }

    // Recover poise over time
    void recover_poise(float dt) {
        time_since_hit += dt;
        if (time_since_hit >= poise_recovery_delay) {
            current_poise += poise_recovery_rate * dt;
            if (current_poise > max_poise) {
                current_poise = max_poise;
            }
        }
    }

    // Reset poise to full
    void reset_poise() {
        current_poise = max_poise;
        time_since_hit = poise_recovery_delay;
    }
};

} // namespace engine::combat
