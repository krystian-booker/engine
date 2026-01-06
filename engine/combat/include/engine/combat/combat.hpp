#pragma once

// Umbrella header for engine::combat module

#include <engine/combat/hitbox.hpp>
#include <engine/combat/hurtbox.hpp>
#include <engine/combat/damage.hpp>
#include <engine/combat/iframe.hpp>
#include <engine/combat/attack_phases.hpp>

#include <engine/scene/world.hpp>
#include <engine/scene/entity.hpp>

namespace engine::combat {

// ============================================================================
// Combat Events
// ============================================================================

// Emitted when damage is dealt
struct DamageDealtEvent {
    DamageInfo info;
};

// Emitted when an entity is hit (before damage calculation)
struct EntityHitEvent {
    scene::Entity attacker;
    scene::Entity target;
    Vec3 hit_point;
    std::string hitbox_id;
    std::string hurtbox_type;
};

// Emitted when attack phase changes
struct AttackPhaseChangedEvent {
    scene::Entity entity;
    AttackPhase old_phase;
    AttackPhase new_phase;
    std::string attack_name;
};

// Emitted when an attack starts
struct AttackStartedEvent {
    scene::Entity entity;
    std::string attack_name;
};

// Emitted when an attack ends (completed or canceled)
struct AttackEndedEvent {
    scene::Entity entity;
    std::string attack_name;
    bool was_canceled;
};

// Emitted when i-frames start
struct IFramesStartedEvent {
    scene::Entity entity;
    float duration;
    IFrameSource source;
};

// Emitted when i-frames end
struct IFramesEndedEvent {
    scene::Entity entity;
    IFrameSource source;
};

// Emitted when an entity is staggered (poise broken)
struct EntityStaggeredEvent {
    scene::Entity entity;
    scene::Entity attacker;
};

// Emitted when a parry occurs
struct ParryEvent {
    scene::Entity defender;
    scene::Entity attacker;
    Vec3 hit_point;
};

// Emitted when damage is blocked
struct BlockEvent {
    scene::Entity defender;
    scene::Entity attacker;
    float blocked_damage;
    float damage_taken;
};

// ============================================================================
// Combat ECS Systems
// ============================================================================

// Hitbox vs Hurtbox collision detection system (FixedUpdate phase)
void hitbox_detection_system(scene::World& world, double dt);

// I-frame timer update system (FixedUpdate phase)
void iframe_system(scene::World& world, double dt);

// Attack phase progression system (Update phase)
void attack_phase_system(scene::World& world, double dt);

// Poise recovery system (Update phase)
void poise_recovery_system(scene::World& world, double dt);

// ============================================================================
// Combat Initialization
// ============================================================================

// Register all combat components with reflection system
void register_combat_components();

// Register combat systems with the scheduler
void register_combat_systems(scene::World& world);

} // namespace engine::combat
