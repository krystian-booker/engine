#pragma once

#include <engine/core/math.hpp>
#include <engine/scene/world.hpp>
#include <vector>
#include <cstdint>

namespace engine::navigation {

using namespace engine::core;

// Types of navigation behaviors
enum class NavBehaviorType : uint8_t {
    None,       // No behavior - manual control
    Wander,     // Random movement within area
    Patrol,     // Follow waypoints in sequence
    Follow,     // Follow a target entity
    Flee        // Move away from a position
};

// Component for automatic navigation behaviors
struct NavBehaviorComponent {
    NavBehaviorType type = NavBehaviorType::None;
    bool enabled = true;

    // =====================
    // Wander settings
    // =====================
    float wander_radius = 10.0f;       // Maximum distance from origin to wander
    float wander_wait_min = 1.0f;      // Minimum wait time at destination
    float wander_wait_max = 3.0f;      // Maximum wait time at destination
    Vec3 wander_origin{0.0f};          // Center point for wandering

    // =====================
    // Patrol settings
    // =====================
    std::vector<Vec3> patrol_points;   // Waypoints to visit
    bool patrol_loop = true;           // Loop (true) or ping-pong (false)
    float patrol_wait_time = 0.0f;     // Wait duration at each point

    // =====================
    // Follow settings
    // =====================
    uint32_t follow_target = 0;        // Entity ID to follow
    float follow_distance = 3.0f;      // Distance to maintain from target
    float follow_update_rate = 0.5f;   // Seconds between path updates

    // =====================
    // Flee settings
    // =====================
    Vec3 flee_from{0.0f};              // Position to flee from
    float flee_distance = 15.0f;       // How far to flee before stopping

    // =====================
    // Runtime state (not serialized)
    // =====================
    size_t patrol_index = 0;           // Current patrol waypoint index
    bool patrol_forward = true;        // Direction for ping-pong patrol
    float wait_timer = 0.0f;           // Current wait countdown
    float follow_timer = 0.0f;         // Time since last follow update
    bool behavior_started = false;     // Has the behavior been started
};

// ECS system function - updates all NavBehaviorComponent entities
// Registered in FixedUpdate phase (priority 3, before nav_agents)
void navigation_behavior_system(scene::World& world, double dt);

} // namespace engine::navigation
