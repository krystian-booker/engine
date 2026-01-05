#pragma once

#include <engine/physics/shapes.hpp>
#include <engine/physics/layers.hpp>
#include <engine/core/math.hpp>
#include <cstdint>

namespace engine::physics {

using namespace engine::core;

// Physics body ID (opaque handle)
struct PhysicsBodyId {
    uint32_t id = UINT32_MAX;
    bool valid() const { return id != UINT32_MAX; }
};

// Body motion types
enum class BodyType : uint8_t {
    Static,     // Never moves (floors, walls)
    Kinematic,  // Moved by code, not affected by forces
    Dynamic     // Fully simulated
};

// Body creation settings
struct BodySettings {
    BodyType type = BodyType::Dynamic;
    ShapeSettings* shape = nullptr;  // Required

    // Initial transform
    Vec3 position{0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};

    // Initial velocities
    Vec3 linear_velocity{0.0f};
    Vec3 angular_velocity{0.0f};

    // Physics properties
    float mass = 1.0f;           // Only for dynamic bodies
    float friction = 0.5f;
    float restitution = 0.0f;    // Bounciness (0-1)
    float linear_damping = 0.05f;
    float angular_damping = 0.05f;

    // Collision settings
    uint16_t layer = layers::DYNAMIC;
    bool is_sensor = false;      // Triggers events but doesn't physically collide
    bool allow_sleep = true;     // Can the body go to sleep when still

    // Constraints
    bool lock_rotation_x = false;
    bool lock_rotation_y = false;
    bool lock_rotation_z = false;

    // User data
    void* user_data = nullptr;
};

// Collision contact point
struct ContactPoint {
    Vec3 position;           // World space contact position
    Vec3 normal;             // Normal pointing from body A to body B
    float penetration_depth; // How far bodies are overlapping
    Vec3 impulse;            // Impulse applied to resolve collision
};

// Collision event
struct CollisionEvent {
    PhysicsBodyId body_a;
    PhysicsBodyId body_b;
    ContactPoint contact;
    bool is_start;  // true = collision started, false = collision ended
};

} // namespace engine::physics
