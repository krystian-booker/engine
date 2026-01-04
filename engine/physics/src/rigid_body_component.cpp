#include <engine/physics/rigid_body_component.hpp>
#include <engine/physics/physics_world.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>

namespace engine::physics {

// Create a physics body from component settings and initial transform
PhysicsBodyId create_rigid_body(PhysicsWorld& world, RigidBodyComponent& rb, const scene::LocalTransform& transform) {
    BodySettings settings;
    settings.type = rb.type;
    settings.shape = rb.get_shape_ptr();
    settings.position = transform.position;
    settings.rotation = transform.rotation;
    settings.mass = rb.mass;
    settings.friction = rb.friction;
    settings.restitution = rb.restitution;
    settings.linear_damping = rb.linear_damping;
    settings.angular_damping = rb.angular_damping;
    settings.layer = rb.layer;
    settings.is_sensor = rb.is_sensor;
    settings.allow_sleep = rb.allow_sleep;
    settings.lock_rotation_x = rb.lock_rotation_x;
    settings.lock_rotation_y = rb.lock_rotation_y;
    settings.lock_rotation_z = rb.lock_rotation_z;
    settings.user_data = rb.user_data;

    return world.create_body(settings);
}

// Destroy a physics body owned by a component
void destroy_rigid_body(PhysicsWorld& world, RigidBodyComponent& rb) {
    if (rb.body_id.valid()) {
        world.destroy_body(rb.body_id);
        rb.body_id = PhysicsBodyId{};
        rb.initialized = false;
    }
}

// System function: initializes new rigid bodies and syncs physics transforms to ECS transforms
void rigid_body_sync_system(scene::World& world, PhysicsWorld& physics, float /*dt*/) {
    auto view = world.view<RigidBodyComponent, scene::LocalTransform>();

    for (auto entity : view) {
        auto& rb = view.get<RigidBodyComponent>(entity);
        auto& transform = view.get<scene::LocalTransform>(entity);

        // Initialize body if needed
        if (!rb.initialized) {
            rb.body_id = create_rigid_body(physics, rb, transform);
            rb.initialized = rb.body_id.valid();
            continue;  // Skip sync on first frame after creation
        }

        // Validate body still exists
        if (!physics.is_valid(rb.body_id)) {
            rb.initialized = false;
            continue;
        }

        // Sync physics transform back to ECS transform
        if (rb.sync_to_transform && rb.type != BodyType::Static) {
            transform.position = physics.get_position(rb.body_id);
            transform.rotation = physics.get_rotation(rb.body_id);
        }
    }
}

} // namespace engine::physics
