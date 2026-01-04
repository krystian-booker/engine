#pragma once

#include <engine/physics/body.hpp>
#include <engine/physics/shapes.hpp>
#include <memory>
#include <variant>

namespace engine::physics {

// Shape variant for storing shape settings inline
// This avoids separate heap allocation for the common shape types
using ShapeVariant = std::variant<
    BoxShapeSettings,
    SphereShapeSettings,
    CapsuleShapeSettings,
    CylinderShapeSettings,
    ConvexHullShapeSettings,
    MeshShapeSettings,
    CompoundShapeSettings
>;

// ECS component for rigid body physics
// Entities with this component will have physics simulation applied
struct RigidBodyComponent {
    PhysicsBodyId body_id;            // Physics body handle (set by system)
    ShapeVariant shape;               // Shape configuration
    BodyType type = BodyType::Dynamic;

    // Physics properties
    float mass = 1.0f;
    float friction = 0.5f;
    float restitution = 0.0f;         // Bounciness (0-1)
    float linear_damping = 0.05f;
    float angular_damping = 0.05f;

    // Collision settings
    uint16_t layer = layers::DYNAMIC;
    bool is_sensor = false;           // Triggers events but doesn't physically collide

    // Behavior
    bool sync_to_transform = true;    // Sync physics position/rotation to transform each frame
    bool allow_sleep = true;          // Can the body sleep when still

    // Rotation locks
    bool lock_rotation_x = false;
    bool lock_rotation_y = false;
    bool lock_rotation_z = false;

    // User data pointer
    void* user_data = nullptr;

    // Internal state (managed by system)
    bool initialized = false;

    // Default constructor - creates a dynamic box
    RigidBodyComponent() : shape(BoxShapeSettings{Vec3{0.5f}}) {}

    // Construct with specific shape
    template<typename T>
    explicit RigidBodyComponent(T&& shape_settings)
        : shape(std::forward<T>(shape_settings)) {}

    // Helper to get ShapeSettings pointer for physics world creation
    ShapeSettings* get_shape_ptr() {
        return std::visit([](auto& s) -> ShapeSettings* { return &s; }, shape);
    }

    const ShapeSettings* get_shape_ptr() const {
        return std::visit([](const auto& s) -> const ShapeSettings* { return &s; }, shape);
    }

    // Convenience setters
    RigidBodyComponent& set_type(BodyType t) { type = t; return *this; }
    RigidBodyComponent& set_mass(float m) { mass = m; return *this; }
    RigidBodyComponent& set_friction(float f) { friction = f; return *this; }
    RigidBodyComponent& set_restitution(float r) { restitution = r; return *this; }
    RigidBodyComponent& set_layer(uint16_t l) { layer = l; return *this; }
    RigidBodyComponent& set_sensor(bool s) { is_sensor = s; return *this; }
    RigidBodyComponent& set_sync(bool s) { sync_to_transform = s; return *this; }
};

// Factory functions for common configurations
inline RigidBodyComponent make_static_box(const Vec3& half_extents) {
    RigidBodyComponent rb{BoxShapeSettings{half_extents}};
    rb.type = BodyType::Static;
    rb.layer = layers::STATIC;
    return rb;
}

inline RigidBodyComponent make_dynamic_box(const Vec3& half_extents, float mass = 1.0f) {
    RigidBodyComponent rb{BoxShapeSettings{half_extents}};
    rb.type = BodyType::Dynamic;
    rb.mass = mass;
    return rb;
}

inline RigidBodyComponent make_dynamic_sphere(float radius, float mass = 1.0f) {
    RigidBodyComponent rb{SphereShapeSettings{radius}};
    rb.type = BodyType::Dynamic;
    rb.mass = mass;
    return rb;
}

inline RigidBodyComponent make_trigger_box(const Vec3& half_extents) {
    RigidBodyComponent rb{BoxShapeSettings{half_extents}};
    rb.type = BodyType::Static;
    rb.is_sensor = true;
    rb.layer = layers::TRIGGER;
    return rb;
}

inline RigidBodyComponent make_trigger_sphere(float radius) {
    RigidBodyComponent rb{SphereShapeSettings{radius}};
    rb.type = BodyType::Static;
    rb.is_sensor = true;
    rb.layer = layers::TRIGGER;
    return rb;
}

} // namespace engine::physics

namespace engine::scene { class World; }

namespace engine::physics {

class PhysicsWorld;

// System to sync rigid bodies with transforms
void rigid_body_sync_system(engine::scene::World& world, PhysicsWorld& physics, float dt);

} // namespace engine::physics
