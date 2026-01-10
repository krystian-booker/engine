#pragma once

// engine::companion module
// AI companions, party management, formations

#include <engine/scene/entity.hpp>
#include <engine/core/math.hpp>
#include <cstdint>
#include <string>
#include <functional>

namespace engine::companion {

using namespace engine::core;

// ============================================================================
// Companion States
// ============================================================================

enum class CompanionState : uint8_t {
    Following,      // Following the leader
    Waiting,        // Staying at position
    Attacking,      // Engaging an enemy
    Defending,      // Protecting leader/position
    Moving,         // Moving to commanded position
    Interacting,    // Interacting with object
    Dead,           // Companion is dead/downed
    Custom          // Game-specific state
};

const char* companion_state_to_string(CompanionState state);

// ============================================================================
// Companion Commands
// ============================================================================

enum class CompanionCommand : uint8_t {
    Follow,         // Resume following leader
    Wait,           // Stay at current position
    Attack,         // Attack a target
    Defend,         // Defend a position or entity
    Move,           // Move to a position
    Interact,       // Interact with an object
    Dismiss,        // Leave the party
    Revive          // Revive from downed state
};

// ============================================================================
// Companion Combat Behavior
// ============================================================================

enum class CombatBehavior : uint8_t {
    Aggressive,     // Attack enemies on sight
    Defensive,      // Only attack if attacked
    Passive,        // Never attack
    Support         // Prioritize healing/buffs
};

// ============================================================================
// Companion Component
// ============================================================================

struct CompanionComponent {
    // Owner/leader entity
    scene::Entity owner = scene::NullEntity;

    // Current state
    CompanionState state = CompanionState::Following;
    CompanionState previous_state = CompanionState::Following;

    // Unique identifier for this companion type
    std::string companion_id;
    std::string display_name;

    // Follow behavior
    float follow_distance = 2.5f;           // Distance to maintain from leader
    float follow_speed_multiplier = 1.0f;   // Speed relative to leader
    float catch_up_speed_multiplier = 1.5f; // Speed when too far behind
    float catch_up_distance = 5.0f;         // Distance to trigger catch-up

    // Teleport behavior
    bool teleport_if_too_far = true;
    float teleport_distance = 30.0f;        // Distance to trigger teleport
    float teleport_cooldown = 5.0f;         // Min time between teleports
    float time_since_teleport = 0.0f;

    // Combat behavior
    CombatBehavior combat_behavior = CombatBehavior::Aggressive;
    bool auto_engage_enemies = true;
    float engagement_range = 15.0f;         // Range to auto-engage
    float disengage_range = 25.0f;          // Range to stop chasing
    float assist_range = 10.0f;             // Range to assist owner in combat

    // Command target (for Move, Attack, etc.)
    Vec3 command_position{0.0f};
    scene::Entity command_target = scene::NullEntity;

    // Wait position (saved when Wait command issued)
    Vec3 wait_position{0.0f};

    // Formation slot (-1 = not in formation)
    int formation_slot = -1;

    // Combat target
    scene::Entity combat_target = scene::NullEntity;
    float time_in_combat = 0.0f;

    // State timers
    float state_time = 0.0f;

    // Flags
    bool is_active = true;                  // Is companion currently active
    bool is_essential = false;              // Cannot be killed (goes to downed state)
    bool can_be_commanded = true;           // Responds to player commands

    // ========================================================================
    // State Queries
    // ========================================================================

    bool is_following() const { return state == CompanionState::Following; }
    bool is_waiting() const { return state == CompanionState::Waiting; }
    bool is_in_combat() const { return state == CompanionState::Attacking || state == CompanionState::Defending; }
    bool is_dead() const { return state == CompanionState::Dead; }
    bool is_idle() const { return state == CompanionState::Following || state == CompanionState::Waiting; }

    // ========================================================================
    // State Transitions
    // ========================================================================

    void set_state(CompanionState new_state) {
        if (state != new_state) {
            previous_state = state;
            state = new_state;
            state_time = 0.0f;
        }
    }
};

// ============================================================================
// Events
// ============================================================================

struct CompanionJoinedEvent {
    scene::Entity companion;
    scene::Entity owner;
};

struct CompanionLeftEvent {
    scene::Entity companion;
    scene::Entity owner;
    bool was_dismissed;
};

struct CompanionStateChangedEvent {
    scene::Entity companion;
    CompanionState old_state;
    CompanionState new_state;
};

struct CompanionCommandedEvent {
    scene::Entity companion;
    CompanionCommand command;
    Vec3 target_position;
    scene::Entity target_entity;
};

struct CompanionDownedEvent {
    scene::Entity companion;
    scene::Entity attacker;
};

struct CompanionRevivedEvent {
    scene::Entity companion;
    scene::Entity reviver;
};

// ============================================================================
// Component Registration
// ============================================================================

void register_companion_components();

} // namespace engine::companion
