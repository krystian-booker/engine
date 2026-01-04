#pragma once

#include <engine/navigation/nav_tile_cache.hpp>
#include <engine/core/math.hpp>

namespace engine::navigation {

using namespace engine::core;

// Component for entities that act as dynamic navigation obstacles
// Pure data struct following ECS conventions
struct NavObstacleComponent {
    // Shape configuration
    ObstacleShape shape = ObstacleShape::Box;

    // For Cylinder: uses position from transform + these values
    float cylinder_radius = 0.5f;
    float cylinder_height = 2.0f;

    // For Box/OrientedBox: half-extents relative to entity center
    Vec3 half_extents{0.5f, 1.0f, 0.5f};

    // Offset from entity transform position
    Vec3 offset{0.0f};

    // Enable/disable the obstacle
    bool enabled = true;

    // Runtime state (managed by system, not serialized)
    NavObstacleHandle handle;            // Current obstacle handle in tile cache
    bool needs_update = true;            // True when transform changed
    Vec3 last_position{0.0f};            // Last synced position
    float last_y_rotation = 0.0f;        // Last synced Y rotation (for oriented box)
};

} // namespace engine::navigation
