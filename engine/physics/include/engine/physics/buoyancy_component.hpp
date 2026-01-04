#pragma once

#include <engine/physics/body.hpp>
#include <vector>

namespace engine::physics {

// Buoyancy calculation mode
enum class BuoyancyMode : uint8_t {
    Automatic,  // Calculate from rigid body shape volume
    Manual,     // Use specified buoyancy points
    Voxel       // Subdivide into voxels for accuracy (slower but more realistic)
};

// Buoyancy sample point for manual mode
struct BuoyancyPoint {
    Vec3 local_position{0.0f};  // Position relative to entity center
    float radius = 0.5f;        // Sample sphere radius
    float volume = 1.0f;        // Displacement volume at this point (m^3)
};

// Buoyancy component - makes rigid bodies float in water
struct BuoyancyComponent {
    BuoyancyMode mode = BuoyancyMode::Automatic;

    // Manual mode: specific buoyancy sample points
    std::vector<BuoyancyPoint> buoyancy_points;

    // Voxel mode settings
    Vec3 voxel_resolution{0.5f, 0.5f, 0.5f};  // Size of each voxel
    uint32_t max_voxels = 64;                  // Limit for performance

    // Automatic mode: volume override
    float volume_override = 0.0f;  // If > 0, use this instead of shape volume

    // Physics properties
    float buoyancy_multiplier = 1.0f;         // Scale buoyancy force
    float water_drag_multiplier = 1.0f;       // Scale water drag
    float linear_damping_in_water = 0.5f;     // Additional linear damping when submerged
    float angular_damping_in_water = 0.3f;    // Additional angular damping when submerged

    // Surface interaction
    float surface_splash_threshold = 2.0f;    // Speed for splash effect
    float surface_exit_threshold = 1.0f;      // Speed for exit splash

    // Stability
    float center_of_buoyancy_offset_y = 0.0f; // Raise/lower buoyancy center for stability
    bool apply_rotational_damping = true;     // Extra damping to prevent spinning

    // Runtime state (set by system)
    float submerged_fraction = 0.0f;          // 0 = above water, 1 = fully submerged
    bool is_in_water = false;
    Vec3 last_buoyancy_force{0.0f};
    Vec3 last_buoyancy_torque{0.0f};

    // Events (for game logic)
    bool just_entered_water = false;          // True for one frame when entering
    bool just_exited_water = false;           // True for one frame when exiting

    // Internal
    bool initialized = false;
};

// Buoyancy calculation result
struct BuoyancyResult {
    Vec3 force{0.0f};           // Total buoyancy force
    Vec3 torque{0.0f};          // Torque from off-center buoyancy
    float submerged_volume = 0.0f;
    float submerged_fraction = 0.0f;
    Vec3 center_of_buoyancy{0.0f};  // World position of buoyancy center
};

// Factory functions for common configurations
inline BuoyancyComponent make_default_buoyancy() {
    return BuoyancyComponent{};
}

inline BuoyancyComponent make_boat_buoyancy() {
    BuoyancyComponent bc;
    bc.mode = BuoyancyMode::Manual;
    bc.buoyancy_multiplier = 1.2f;
    bc.apply_rotational_damping = true;
    bc.angular_damping_in_water = 0.5f;
    return bc;
}

inline BuoyancyComponent make_heavy_object_buoyancy(float density_ratio = 0.8f) {
    BuoyancyComponent bc;
    bc.buoyancy_multiplier = density_ratio;  // < 1.0 will sink
    return bc;
}

inline BuoyancyComponent make_flotsam_buoyancy() {
    BuoyancyComponent bc;
    bc.buoyancy_multiplier = 1.5f;  // Very buoyant
    bc.water_drag_multiplier = 1.5f;
    bc.angular_damping_in_water = 0.8f;  // Stabilize quickly
    return bc;
}

} // namespace engine::physics
