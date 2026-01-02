#pragma once

#include <engine/physics/body.hpp>
#include <engine/physics/shapes.hpp>
#include <engine/physics/layers.hpp>
#include <engine/core/project_settings.hpp>
#include <memory>
#include <vector>
#include <functional>

namespace engine::physics {

// Raycast hit result
struct RaycastHit {
    PhysicsBodyId body;
    Vec3 point;
    Vec3 normal;
    float distance;
    bool hit = false;
};

// Collision callback type
using CollisionCallback = std::function<void(const CollisionEvent&)>;

// Constraint ID (opaque handle)
struct ConstraintId {
    uint32_t id = UINT32_MAX;
    bool valid() const { return id != UINT32_MAX; }
};

// Constraint settings for different joint types
struct FixedConstraintSettings {
    PhysicsBodyId body_a;
    PhysicsBodyId body_b;
    Vec3 local_anchor_a{0.0f};  // Anchor point in body A's local space
    Vec3 local_anchor_b{0.0f};  // Anchor point in body B's local space
};

struct HingeConstraintSettings {
    PhysicsBodyId body_a;
    PhysicsBodyId body_b;
    Vec3 local_anchor_a{0.0f};
    Vec3 local_anchor_b{0.0f};
    Vec3 hinge_axis{0.0f, 1.0f, 0.0f};  // Axis in body A's local space
    float limit_min = -3.14159f;         // Min angle in radians
    float limit_max = 3.14159f;          // Max angle in radians
    bool enable_limits = true;
};

struct SwingTwistConstraintSettings {
    PhysicsBodyId body_a;
    PhysicsBodyId body_b;
    Vec3 local_anchor_a{0.0f};
    Vec3 local_anchor_b{0.0f};
    Vec3 twist_axis{0.0f, 1.0f, 0.0f};   // Twist axis in body A's local space
    Vec3 plane_axis{1.0f, 0.0f, 0.0f};   // Plane axis in body A's local space
    float swing_limit_y = 0.5f;           // Half cone angle around Y (radians)
    float swing_limit_z = 0.5f;           // Half cone angle around Z (radians)
    float twist_min = -0.5f;              // Min twist angle (radians)
    float twist_max = 0.5f;               // Max twist angle (radians)
};

// Body shape information for debug rendering
struct BodyShapeInfo {
    ShapeType type = ShapeType::Box;
    Vec3 dimensions{0.5f};       // Half-extents for box, radius/height for others
    Vec3 center_offset{0.0f};
};

// Contact point for debug rendering
struct ContactPointInfo {
    Vec3 position;
    Vec3 normal;
    float penetration_depth;
    PhysicsBodyId body_a;
    PhysicsBodyId body_b;
};

// Constraint info for debug rendering
struct ConstraintInfo {
    ConstraintId id;
    PhysicsBodyId body_a;
    PhysicsBodyId body_b;
    Vec3 world_anchor_a;
    Vec3 world_anchor_b;
};

// Physics world - manages all physics simulation
class PhysicsWorld {
public:
    struct Impl;

    PhysicsWorld();
    ~PhysicsWorld();

    // Non-copyable
    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    // Movable
    PhysicsWorld(PhysicsWorld&&) noexcept;
    PhysicsWorld& operator=(PhysicsWorld&&) noexcept;

    // Initialization
    void init(const engine::core::PhysicsSettings& settings);
    void shutdown();

    // Simulation
    void step(double dt);  // Call from fixed update

    // Body management
    PhysicsBodyId create_body(const BodySettings& settings);
    void destroy_body(PhysicsBodyId id);
    bool is_valid(PhysicsBodyId id) const;

    // Body transform
    void set_position(PhysicsBodyId id, const Vec3& pos);
    void set_rotation(PhysicsBodyId id, const Quat& rot);
    void set_transform(PhysicsBodyId id, const Vec3& pos, const Quat& rot);
    Vec3 get_position(PhysicsBodyId id) const;
    Quat get_rotation(PhysicsBodyId id) const;

    // Body velocity
    void set_linear_velocity(PhysicsBodyId id, const Vec3& vel);
    void set_angular_velocity(PhysicsBodyId id, const Vec3& vel);
    Vec3 get_linear_velocity(PhysicsBodyId id) const;
    Vec3 get_angular_velocity(PhysicsBodyId id) const;

    // Forces and impulses
    void add_force(PhysicsBodyId id, const Vec3& force);
    void add_force_at_point(PhysicsBodyId id, const Vec3& force, const Vec3& point);
    void add_torque(PhysicsBodyId id, const Vec3& torque);
    void add_impulse(PhysicsBodyId id, const Vec3& impulse);
    void add_impulse_at_point(PhysicsBodyId id, const Vec3& impulse, const Vec3& point);

    // Body properties
    void set_gravity_factor(PhysicsBodyId id, float factor);
    void set_friction(PhysicsBodyId id, float friction);
    void set_restitution(PhysicsBodyId id, float restitution);
    void activate_body(PhysicsBodyId id);  // Wake up sleeping body
    bool is_active(PhysicsBodyId id) const;

    // Motion type control (for ragdoll kinematic/dynamic switching)
    void set_motion_type(PhysicsBodyId id, BodyType type);
    BodyType get_motion_type(PhysicsBodyId id) const;

    // Body shape queries (for debug rendering)
    BodyShapeInfo get_body_shape_info(PhysicsBodyId id) const;
    BodyType get_body_type(PhysicsBodyId id) const;

    // Queries
    RaycastHit raycast(const Vec3& origin, const Vec3& direction, float max_distance,
                       uint16_t layer_mask = 0xFFFF) const;
    std::vector<RaycastHit> raycast_all(const Vec3& origin, const Vec3& direction,
                                        float max_distance, uint16_t layer_mask = 0xFFFF) const;
    std::vector<PhysicsBodyId> overlap_sphere(const Vec3& center, float radius,
                                              uint16_t layer_mask = 0xFFFF) const;
    std::vector<PhysicsBodyId> overlap_box(const Vec3& center, const Vec3& half_extents,
                                           const Quat& rotation, uint16_t layer_mask = 0xFFFF) const;

    // Collision callbacks
    void set_collision_callback(CollisionCallback callback);

    // Collision filter
    CollisionFilter& get_collision_filter();
    const CollisionFilter& get_collision_filter() const;

    // Constraint management
    ConstraintId create_fixed_constraint(const FixedConstraintSettings& settings);
    ConstraintId create_hinge_constraint(const HingeConstraintSettings& settings);
    ConstraintId create_swing_twist_constraint(const SwingTwistConstraintSettings& settings);
    void destroy_constraint(ConstraintId id);

    // Constraint motor control
    void set_constraint_motor_state(ConstraintId id, bool enabled);
    void set_constraint_motor_target(ConstraintId id, const Quat& target_rotation);
    void set_constraint_motor_velocity(ConstraintId id, const Vec3& angular_velocity);
    void set_constraint_motor_strength(ConstraintId id, float max_force_limit);

    // Debug/contact queries
    std::vector<ContactPointInfo> get_contact_points() const;
    std::vector<ConstraintInfo> get_all_constraints() const;

    // Settings
    void set_gravity(const Vec3& gravity);
    Vec3 get_gravity() const;

    // Statistics
    uint32_t get_body_count() const;
    uint32_t get_active_body_count() const;

    // Body iteration (for debug rendering)
    std::vector<PhysicsBodyId> get_all_body_ids() const;

private:
    std::unique_ptr<Impl> m_impl;

    // Friend declarations for implementation functions (in jolt_impl.cpp)
    friend Impl* create_physics_impl();
    friend void destroy_physics_impl(Impl*);
    friend void init_physics_impl(Impl*, const engine::core::PhysicsSettings&);
    friend void shutdown_physics_impl(Impl*);
    friend void step_physics_impl(Impl*, double);
    friend PhysicsBodyId create_body_impl(Impl*, const BodySettings&);
    friend void destroy_body_impl(Impl*, PhysicsBodyId);
    friend bool is_valid_impl(Impl*, PhysicsBodyId);
    friend void set_position_impl(Impl*, PhysicsBodyId, const Vec3&);
    friend void set_rotation_impl(Impl*, PhysicsBodyId, const Quat&);
    friend Vec3 get_position_impl(Impl*, PhysicsBodyId);
    friend Quat get_rotation_impl(Impl*, PhysicsBodyId);
    friend void set_linear_velocity_impl(Impl*, PhysicsBodyId, const Vec3&);
    friend void set_angular_velocity_impl(Impl*, PhysicsBodyId, const Vec3&);
    friend Vec3 get_linear_velocity_impl(Impl*, PhysicsBodyId);
    friend Vec3 get_angular_velocity_impl(Impl*, PhysicsBodyId);
    friend void add_force_impl(Impl*, PhysicsBodyId, const Vec3&);
    friend void add_impulse_impl(Impl*, PhysicsBodyId, const Vec3&);
    friend RaycastHit raycast_impl(Impl*, const Vec3&, const Vec3&, float, uint16_t);
    friend void set_gravity_impl(Impl*, const Vec3&);
    friend Vec3 get_gravity_impl(Impl*);
    friend uint32_t get_body_count_impl(Impl*);
    friend CollisionFilter& get_collision_filter_impl(Impl*);

    // New impl functions for constraint and motion type support
    friend void set_motion_type_impl(Impl*, PhysicsBodyId, BodyType);
    friend BodyType get_motion_type_impl(Impl*, PhysicsBodyId);
    friend BodyShapeInfo get_body_shape_info_impl(Impl*, PhysicsBodyId);
    friend ConstraintId create_fixed_constraint_impl(Impl*, const FixedConstraintSettings&);
    friend ConstraintId create_hinge_constraint_impl(Impl*, const HingeConstraintSettings&);
    friend ConstraintId create_swing_twist_constraint_impl(Impl*, const SwingTwistConstraintSettings&);
    friend void destroy_constraint_impl(Impl*, ConstraintId);
    friend void set_constraint_motor_state_impl(Impl*, ConstraintId, bool);
    friend void set_constraint_motor_target_impl(Impl*, ConstraintId, const Quat&);
    friend void set_constraint_motor_velocity_impl(Impl*, ConstraintId, const Vec3&);
    friend void set_constraint_motor_strength_impl(Impl*, ConstraintId, float);
    friend std::vector<ContactPointInfo> get_contact_points_impl(Impl*);
    friend std::vector<ConstraintInfo> get_all_constraints_impl(Impl*);
};

} // namespace engine::physics
