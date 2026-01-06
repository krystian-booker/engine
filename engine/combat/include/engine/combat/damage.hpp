#pragma once

#include <engine/scene/entity.hpp>
#include <engine/scene/world.hpp>
#include <engine/core/math.hpp>
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace engine::combat {

struct HitboxComponent;
struct HurtboxComponent;

// Complete damage event data
struct DamageInfo {
    scene::Entity source = scene::NullEntity;   // Who dealt the damage
    scene::Entity target = scene::NullEntity;   // Who received it

    // Damage values
    float raw_damage = 0.0f;                    // Before any modifiers
    float final_damage = 0.0f;                  // After all modifiers applied
    std::string damage_type = "physical";

    // Hit location
    Vec3 hit_point{0.0f};                       // World space position
    Vec3 hit_normal{0.0f, 1.0f, 0.0f};          // Surface normal
    Vec3 knockback{0.0f};                       // Final knockback vector

    // Poise/stagger
    float poise_damage = 0.0f;
    bool caused_stagger = false;

    // Special flags
    bool is_critical = false;
    bool is_blocked = false;
    bool is_parried = false;
    bool is_backstab = false;

    // Source info
    std::string hitbox_id;
    std::string hurtbox_type;
    std::string attack_name;
};

// Damage modifier callback (for buffs, debuffs, armor, etc.)
// Returns modified damage amount, can modify DamageInfo for flags
using DamageModifier = std::function<void(DamageInfo&)>;

// Damage calculation and application
class DamageSystem {
public:
    static DamageSystem& instance();

    // Delete copy/move
    DamageSystem(const DamageSystem&) = delete;
    DamageSystem& operator=(const DamageSystem&) = delete;

    // Calculate damage (does not apply, just computes)
    DamageInfo calculate_damage(
        scene::World& world,
        scene::Entity source,
        scene::Entity target,
        const HitboxComponent& hitbox,
        const HurtboxComponent& hurtbox,
        const Vec3& hit_point,
        const Vec3& hit_normal
    );

    // Apply calculated damage (triggers events, returns final info)
    DamageInfo apply_damage(scene::World& world, const DamageInfo& info);

    // One-shot calculate + apply
    DamageInfo deal_damage(
        scene::World& world,
        scene::Entity source,
        scene::Entity target,
        const HitboxComponent& hitbox,
        const HurtboxComponent& hurtbox,
        const Vec3& hit_point,
        const Vec3& hit_normal
    );

    // Register global damage modifiers
    void add_modifier(const std::string& name, DamageModifier modifier, int priority = 0);
    void remove_modifier(const std::string& name);
    void clear_modifiers();

    // Hitstop management (brief pause on hit for impact feel)
    void trigger_hitstop(float duration);
    void update_hitstop(float dt);
    float get_hitstop_time_scale() const;       // Returns 0.0 during hitstop, 1.0 otherwise
    bool is_hitstop_active() const { return m_hitstop_remaining > 0.0f; }

    // Hitstop configuration
    void set_default_hitstop_duration(float duration) { m_default_hitstop = duration; }
    void set_hitstop_enabled(bool enabled) { m_hitstop_enabled = enabled; }

private:
    DamageSystem();
    ~DamageSystem() = default;

    void apply_modifiers(DamageInfo& info);
    bool check_backstab(scene::World& world, scene::Entity source, scene::Entity target, float threshold);

    struct ModifierEntry {
        std::string name;
        DamageModifier modifier;
        int priority;
    };

    std::vector<ModifierEntry> m_modifiers;

    // Hitstop state
    float m_hitstop_remaining = 0.0f;
    float m_default_hitstop = 0.05f;
    bool m_hitstop_enabled = true;
};

// Convenience accessor
inline DamageSystem& damage() {
    return DamageSystem::instance();
}

} // namespace engine::combat
