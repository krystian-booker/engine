#include <engine/physics/physics_world.hpp>
#include <engine/core/log.hpp>

namespace engine::physics {

using namespace engine::core;

// Forward declaration of Impl (defined in jolt_impl.cpp)
struct PhysicsWorld::Impl;

// These are implemented in jolt_impl.cpp
extern PhysicsWorld::Impl* create_physics_impl();
extern void destroy_physics_impl(PhysicsWorld::Impl* impl);
extern void init_physics_impl(PhysicsWorld::Impl* impl, const PhysicsSettings& settings);
extern void shutdown_physics_impl(PhysicsWorld::Impl* impl);
extern void step_physics_impl(PhysicsWorld::Impl* impl, double dt);
extern PhysicsBodyId create_body_impl(PhysicsWorld::Impl* impl, const BodySettings& settings);
extern void destroy_body_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id);
extern bool is_valid_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id);
extern void set_position_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, const Vec3& pos);
extern void set_rotation_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, const Quat& rot);
extern Vec3 get_position_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id);
extern Quat get_rotation_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id);
extern void set_linear_velocity_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, const Vec3& vel);
extern void set_angular_velocity_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, const Vec3& vel);
extern Vec3 get_linear_velocity_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id);
extern Vec3 get_angular_velocity_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id);
extern void add_force_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, const Vec3& force);
extern void add_force_at_point_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, const Vec3& force, const Vec3& point);
extern void add_torque_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, const Vec3& torque);
extern void add_impulse_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, const Vec3& impulse);
extern void add_impulse_at_point_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, const Vec3& impulse, const Vec3& point);
extern void set_friction_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, float friction);
extern void set_restitution_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, float restitution);
extern void set_gravity_factor_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id, float factor);
extern void activate_body_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id);
extern bool is_active_impl(PhysicsWorld::Impl* impl, PhysicsBodyId id);
extern RaycastHit raycast_impl(PhysicsWorld::Impl* impl, const Vec3& origin, const Vec3& dir, float max_dist, uint16_t mask);
extern std::vector<RaycastHit> raycast_all_impl(PhysicsWorld::Impl* impl, const Vec3& origin, const Vec3& dir, float max_dist, uint16_t mask);
extern std::vector<PhysicsBodyId> overlap_sphere_impl(PhysicsWorld::Impl* impl, const Vec3& center, float radius, uint16_t mask);
extern std::vector<PhysicsBodyId> overlap_box_impl(PhysicsWorld::Impl* impl, const Vec3& center, const Vec3& half_extents, const Quat& rotation, uint16_t mask);
extern void set_collision_callback_impl(PhysicsWorld::Impl* impl, CollisionCallback callback);
extern void set_gravity_impl(PhysicsWorld::Impl* impl, const Vec3& gravity);
extern Vec3 get_gravity_impl(PhysicsWorld::Impl* impl);
extern uint32_t get_body_count_impl(PhysicsWorld::Impl* impl);
extern uint32_t get_active_body_count_impl(PhysicsWorld::Impl* impl);
extern CollisionFilter& get_collision_filter_impl(PhysicsWorld::Impl* impl);
extern std::vector<PhysicsBodyId> get_all_body_ids_impl(PhysicsWorld::Impl* impl);

// Constructor and destructor defined in jolt_impl.cpp where Impl is complete

// Move operations need to be in jolt_impl.cpp too since they may destroy Impl

void PhysicsWorld::init(const PhysicsSettings& settings) {
    init_physics_impl(m_impl.get(), settings);
    log(LogLevel::Info, "Physics world initialized");
}

void PhysicsWorld::shutdown() {
    shutdown_physics_impl(m_impl.get());
    log(LogLevel::Info, "Physics world shutdown");
}

void PhysicsWorld::step(double dt) {
    step_physics_impl(m_impl.get(), dt);
}

PhysicsBodyId PhysicsWorld::create_body(const BodySettings& settings) {
    return create_body_impl(m_impl.get(), settings);
}

void PhysicsWorld::destroy_body(PhysicsBodyId id) {
    destroy_body_impl(m_impl.get(), id);
}

bool PhysicsWorld::is_valid(PhysicsBodyId id) const {
    return is_valid_impl(m_impl.get(), id);
}

void PhysicsWorld::set_position(PhysicsBodyId id, const Vec3& pos) {
    set_position_impl(m_impl.get(), id, pos);
}

void PhysicsWorld::set_rotation(PhysicsBodyId id, const Quat& rot) {
    set_rotation_impl(m_impl.get(), id, rot);
}

void PhysicsWorld::set_transform(PhysicsBodyId id, const Vec3& pos, const Quat& rot) {
    set_position_impl(m_impl.get(), id, pos);
    set_rotation_impl(m_impl.get(), id, rot);
}

Vec3 PhysicsWorld::get_position(PhysicsBodyId id) const {
    return get_position_impl(m_impl.get(), id);
}

Quat PhysicsWorld::get_rotation(PhysicsBodyId id) const {
    return get_rotation_impl(m_impl.get(), id);
}

void PhysicsWorld::set_linear_velocity(PhysicsBodyId id, const Vec3& vel) {
    set_linear_velocity_impl(m_impl.get(), id, vel);
}

void PhysicsWorld::set_angular_velocity(PhysicsBodyId id, const Vec3& vel) {
    set_angular_velocity_impl(m_impl.get(), id, vel);
}

Vec3 PhysicsWorld::get_linear_velocity(PhysicsBodyId id) const {
    return get_linear_velocity_impl(m_impl.get(), id);
}

Vec3 PhysicsWorld::get_angular_velocity(PhysicsBodyId id) const {
    return get_angular_velocity_impl(m_impl.get(), id);
}

void PhysicsWorld::add_force(PhysicsBodyId id, const Vec3& force) {
    add_force_impl(m_impl.get(), id, force);
}

void PhysicsWorld::add_force_at_point(PhysicsBodyId id, const Vec3& force, const Vec3& point) {
    add_force_at_point_impl(m_impl.get(), id, force, point);
}

void PhysicsWorld::add_torque(PhysicsBodyId id, const Vec3& torque) {
    add_torque_impl(m_impl.get(), id, torque);
}

void PhysicsWorld::add_impulse(PhysicsBodyId id, const Vec3& impulse) {
    add_impulse_impl(m_impl.get(), id, impulse);
}

void PhysicsWorld::add_impulse_at_point(PhysicsBodyId id, const Vec3& impulse, const Vec3& point) {
    add_impulse_at_point_impl(m_impl.get(), id, impulse, point);
}

void PhysicsWorld::set_gravity_factor(PhysicsBodyId id, float factor) {
    set_gravity_factor_impl(m_impl.get(), id, factor);
}

void PhysicsWorld::set_friction(PhysicsBodyId id, float friction) {
    set_friction_impl(m_impl.get(), id, friction);
}

void PhysicsWorld::set_restitution(PhysicsBodyId id, float restitution) {
    set_restitution_impl(m_impl.get(), id, restitution);
}

void PhysicsWorld::activate_body(PhysicsBodyId id) {
    activate_body_impl(m_impl.get(), id);
}

bool PhysicsWorld::is_active(PhysicsBodyId id) const {
    return is_active_impl(m_impl.get(), id);
}

RaycastHit PhysicsWorld::raycast(const Vec3& origin, const Vec3& direction,
                                  float max_distance, uint16_t layer_mask) const {
    return raycast_impl(m_impl.get(), origin, direction, max_distance, layer_mask);
}

std::vector<RaycastHit> PhysicsWorld::raycast_all(const Vec3& origin, const Vec3& direction,
                                                   float max_distance, uint16_t layer_mask) const {
    return raycast_all_impl(m_impl.get(), origin, direction, max_distance, layer_mask);
}

std::vector<PhysicsBodyId> PhysicsWorld::overlap_sphere(const Vec3& center, float radius,
                                                        uint16_t layer_mask) const {
    return overlap_sphere_impl(m_impl.get(), center, radius, layer_mask);
}

std::vector<PhysicsBodyId> PhysicsWorld::overlap_box(const Vec3& center, const Vec3& half_extents,
                                                     const Quat& rotation, uint16_t layer_mask) const {
    return overlap_box_impl(m_impl.get(), center, half_extents, rotation, layer_mask);
}

void PhysicsWorld::set_collision_callback(CollisionCallback callback) {
    set_collision_callback_impl(m_impl.get(), std::move(callback));
}

CollisionFilter& PhysicsWorld::get_collision_filter() {
    return get_collision_filter_impl(m_impl.get());
}

const CollisionFilter& PhysicsWorld::get_collision_filter() const {
    return get_collision_filter_impl(m_impl.get());
}

void PhysicsWorld::set_gravity(const Vec3& gravity) {
    set_gravity_impl(m_impl.get(), gravity);
}

Vec3 PhysicsWorld::get_gravity() const {
    return get_gravity_impl(m_impl.get());
}

uint32_t PhysicsWorld::get_body_count() const {
    return get_body_count_impl(m_impl.get());
}

uint32_t PhysicsWorld::get_active_body_count() const {
    return get_active_body_count_impl(m_impl.get());
}

std::vector<PhysicsBodyId> PhysicsWorld::get_all_body_ids() const {
    return get_all_body_ids_impl(m_impl.get());
}

} // namespace engine::physics
