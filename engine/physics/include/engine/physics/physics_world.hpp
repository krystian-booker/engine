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
};

} // namespace engine::physics
