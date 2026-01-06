#pragma once

#include <engine/effects/effect_definition.hpp>
#include <engine/effects/effect_instance.hpp>
#include <engine/scene/entity.hpp>
#include <string>

namespace engine::effects {

// ============================================================================
// Effect Application Events
// ============================================================================

// Effect was applied to an entity
struct EffectAppliedEvent {
    scene::Entity target;
    scene::Entity source;
    std::string effect_id;
    core::UUID instance_id;
    int initial_stacks;
    float duration;
    bool was_refresh;           // True if refreshed existing effect
    bool was_stack_add;         // True if added to existing stacks
};

// Effect application was blocked
struct EffectBlockedEvent {
    scene::Entity target;
    scene::Entity source;
    std::string effect_id;
    std::string blocked_by;     // What blocked it (immunity, other effect)
};

// ============================================================================
// Effect Removal Events
// ============================================================================

// Effect was removed from an entity
struct EffectRemovedEvent {
    scene::Entity target;
    std::string effect_id;
    core::UUID instance_id;
    RemovalReason reason;
    float remaining_duration;   // How much time was left
    int final_stacks;
};

// Effect expired naturally
struct EffectExpiredEvent {
    scene::Entity target;
    std::string effect_id;
    core::UUID instance_id;
    int final_stacks;
    float total_duration;       // Total time it was active
};

// Effect was dispelled
struct EffectDispelledEvent {
    scene::Entity target;
    scene::Entity dispeller;
    std::string effect_id;
    core::UUID instance_id;
    int stacks_removed;
};

// ============================================================================
// Effect Stack Events
// ============================================================================

// Stacks were added
struct EffectStackAddedEvent {
    scene::Entity target;
    std::string effect_id;
    core::UUID instance_id;
    int old_stacks;
    int new_stacks;
    int stacks_added;
};

// Stacks were removed
struct EffectStackRemovedEvent {
    scene::Entity target;
    std::string effect_id;
    core::UUID instance_id;
    int old_stacks;
    int new_stacks;
    int stacks_removed;
};

// Max stacks reached
struct EffectMaxStacksEvent {
    scene::Entity target;
    std::string effect_id;
    core::UUID instance_id;
    int max_stacks;
};

// ============================================================================
// Effect Duration Events
// ============================================================================

// Duration was refreshed
struct EffectRefreshedEvent {
    scene::Entity target;
    std::string effect_id;
    core::UUID instance_id;
    float old_remaining;
    float new_remaining;
};

// Duration was extended
struct EffectExtendedEvent {
    scene::Entity target;
    std::string effect_id;
    core::UUID instance_id;
    float amount_extended;
    float new_remaining;
};

// ============================================================================
// Effect Tick Events
// ============================================================================

// Effect ticked (for DoT/HoT)
struct EffectTickEvent {
    scene::Entity target;
    std::string effect_id;
    core::UUID instance_id;
    int tick_number;
    float damage_dealt;
    float healing_done;
    int current_stacks;
};

// Damage was dealt by an effect
struct EffectDamageEvent {
    scene::Entity target;
    scene::Entity source;
    std::string effect_id;
    std::string damage_type;
    float damage_amount;
    float remaining_health;
    bool is_lethal;
};

// Healing was done by an effect
struct EffectHealEvent {
    scene::Entity target;
    scene::Entity source;
    std::string effect_id;
    float heal_amount;
    float new_health;
    float max_health;
};

// ============================================================================
// Immunity Events
// ============================================================================

// Immunity was granted
struct ImmunityGrantedEvent {
    scene::Entity entity;
    std::string effect_id;      // Empty if category immunity
    EffectCategory category;    // Category if category immunity
    std::string tag;            // Tag if tag immunity
};

// Immunity was revoked
struct ImmunityRevokedEvent {
    scene::Entity entity;
    std::string effect_id;
    EffectCategory category;
    std::string tag;
};

// ============================================================================
// Aura Events
// ============================================================================

// Entity entered aura range
struct AuraEnteredEvent {
    scene::Entity aura_source;
    scene::Entity affected_entity;
    std::string effect_id;
};

// Entity left aura range
struct AuraLeftEvent {
    scene::Entity aura_source;
    scene::Entity affected_entity;
    std::string effect_id;
};

} // namespace engine::effects
