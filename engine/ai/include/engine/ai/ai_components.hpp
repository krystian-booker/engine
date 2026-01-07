#pragma once

#include <engine/core/math.hpp>
#include <engine/ai/blackboard.hpp>
#include <engine/ai/behavior_tree.hpp>
#include <engine/scene/entity.hpp>
#include <string>
#include <memory>
#include <cstdint>

namespace engine::ai {

using core::Vec3;

// ============================================================================
// AI Controller Component
// ============================================================================

struct AIControllerComponent {
    bool enabled = true;

    // Behavior tree
    BehaviorTreePtr behavior_tree;
    std::unique_ptr<Blackboard> blackboard;

    // Update rate
    float update_interval = 0.1f;               // How often to tick BT (seconds)
    float time_since_update = 0.0f;

    // State tracking
    std::string current_state;                  // For debugging
    BTStatus last_status = BTStatus::Failure;

    // Target tracking
    scene::Entity current_target = scene::NullEntity;
    float time_with_target = 0.0f;

    // Initialize if needed
    void ensure_blackboard() {
        if (!blackboard) {
            blackboard = std::make_unique<Blackboard>();
        }
    }

    // Check if ready to update
    bool should_update(float dt) {
        time_since_update += dt;
        if (time_since_update >= update_interval) {
            time_since_update = 0.0f;
            return true;
        }
        return false;
    }
};

// ============================================================================
// AI Combat Component
// For action game AI combat behavior
// ============================================================================

struct AICombatComponent {
    // Target
    scene::Entity threat = scene::NullEntity;
    float threat_level = 0.0f;                  // How threatening the target is

    // Combat parameters
    float attack_range = 2.0f;                  // Range for melee attacks
    float ranged_attack_range = 15.0f;          // Range for ranged attacks
    float preferred_distance = 3.0f;            // Ideal distance from target
    float min_distance = 1.0f;                  // Don't get closer than this
    float max_chase_distance = 30.0f;           // Give up chase beyond this

    // Attack timing
    float attack_cooldown = 1.5f;
    float time_since_attack = 0.0f;
    float combo_window = 0.5f;
    int current_combo = 0;
    int max_combo = 3;

    // Defense
    float block_chance = 0.3f;                  // Chance to block incoming attack
    float dodge_chance = 0.2f;                  // Chance to dodge
    float parry_window = 0.1f;                  // Time window to parry

    // Behavior weights (0-1)
    float aggression = 0.7f;                    // Higher = more offensive
    float caution = 0.5f;                       // Higher = more defensive
    float patience = 0.5f;                      // Higher = waits for openings

    // Thresholds
    float flee_health_threshold = 0.2f;         // Flee when health below this
    float stagger_threshold = 30.0f;            // Poise damage to stagger

    // State
    bool is_attacking = false;
    bool is_blocking = false;
    bool is_staggered = false;
    bool is_fleeing = false;

    // Attack selection
    std::vector<std::string> available_attacks;
    std::string current_attack;
    int attack_pattern_index = 0;

    // Helpers
    bool can_attack() const {
        return time_since_attack >= attack_cooldown && !is_attacking && !is_staggered;
    }

    bool in_attack_range(float distance) const {
        return distance <= attack_range;
    }

    bool in_ranged_range(float distance) const {
        return distance <= ranged_attack_range && distance > attack_range;
    }

    void start_attack() {
        is_attacking = true;
        time_since_attack = 0.0f;
    }

    void end_attack() {
        is_attacking = false;
        current_combo++;
        if (current_combo >= max_combo) {
            current_combo = 0;
        }
    }
};

// ============================================================================
// AI Patrol Component
// For patrol/idle behavior
// ============================================================================

struct AIPatrolComponent {
    // Patrol type
    enum class PatrolType : uint8_t {
        None,           // Stay in place
        Loop,           // Loop through waypoints
        PingPong,       // Go back and forth
        Random          // Random waypoint selection
    };
    PatrolType type = PatrolType::Loop;

    // Waypoints (positions in world space)
    std::vector<Vec3> waypoints;
    int current_waypoint = 0;
    bool reverse_direction = false;             // For ping-pong

    // Timing
    float wait_time_min = 1.0f;                 // Min time to wait at waypoint
    float wait_time_max = 3.0f;                 // Max time to wait at waypoint
    float current_wait_time = 0.0f;
    float time_at_waypoint = 0.0f;

    // Movement
    float patrol_speed = 2.0f;                  // Walking speed during patrol
    float arrival_distance = 0.5f;              // How close to get to waypoint

    // State
    bool is_waiting = false;
    bool patrol_active = true;

    // Get current target waypoint
    Vec3 get_current_waypoint() const {
        if (waypoints.empty()) return Vec3(0.0f);
        return waypoints[current_waypoint % waypoints.size()];
    }

    // Move to next waypoint
    void advance_waypoint() {
        if (waypoints.empty()) return;

        switch (type) {
            case PatrolType::Loop:
                current_waypoint = (current_waypoint + 1) % static_cast<int>(waypoints.size());
                break;

            case PatrolType::PingPong:
                if (reverse_direction) {
                    current_waypoint--;
                    if (current_waypoint <= 0) {
                        current_waypoint = 0;
                        reverse_direction = false;
                    }
                } else {
                    current_waypoint++;
                    if (current_waypoint >= static_cast<int>(waypoints.size()) - 1) {
                        current_waypoint = static_cast<int>(waypoints.size()) - 1;
                        reverse_direction = true;
                    }
                }
                break;

            case PatrolType::Random: {
                if (waypoints.size() > 1) {
                    int new_wp;
                    do {
                        new_wp = rand() % static_cast<int>(waypoints.size());
                    } while (new_wp == current_waypoint);
                    current_waypoint = new_wp;
                }
                break;
            }

            default:
                break;
        }
    }
};

// ============================================================================
// AI Investigate Component
// For investigating suspicious activity
// ============================================================================

struct AIInvestigateComponent {
    bool is_investigating = false;
    Vec3 investigation_point{0.0f};
    float investigation_time = 0.0f;
    float max_investigation_time = 10.0f;
    float search_radius = 5.0f;
    int search_points_checked = 0;
    int max_search_points = 3;
};

// ============================================================================
// AI Events
// ============================================================================

struct AIStateChangedEvent {
    scene::Entity entity;
    std::string old_state;
    std::string new_state;
};

struct AITargetChangedEvent {
    scene::Entity entity;
    scene::Entity old_target;
    scene::Entity new_target;
};



} // namespace engine::ai
