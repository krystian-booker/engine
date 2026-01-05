#include <engine/script/bindings.hpp>
#include <engine/script/script_context.hpp>
#include <engine/scene/world.hpp>
#include <engine/physics/physics_world.hpp>
#include <engine/physics/rigid_body_component.hpp>
#include <engine/core/log.hpp>

namespace engine::script {

void register_physics_bindings(sol::state& lua) {
    using namespace engine::physics;
    using namespace engine::core;

    // RaycastHit result type
    lua.new_usertype<RaycastHit>("RaycastHit",
        sol::constructors<>(),
        "hit", &RaycastHit::hit,
        "point", &RaycastHit::point,
        "normal", &RaycastHit::normal,
        "distance", &RaycastHit::distance,
        "body", sol::readonly(&RaycastHit::body)
    );

    // PhysicsBodyId (for advanced use cases)
    lua.new_usertype<PhysicsBodyId>("PhysicsBodyId",
        sol::constructors<>(),
        "valid", &PhysicsBodyId::valid,
        "id", sol::readonly(&PhysicsBodyId::id)
    );

    // Create Physics table
    auto physics = lua.create_named_table("Physics");

    // Raycast - single hit
    physics.set_function("raycast", [](const Vec3& origin, const Vec3& direction,
                                        float max_distance, sol::optional<uint16_t> layer_mask) -> RaycastHit {
        auto* world = get_script_context().physics_world;
        if (!world) {
            core::log(core::LogLevel::Warn, "Physics.raycast called without physics context");
            return RaycastHit{};
        }
        return world->raycast(origin, direction, max_distance, layer_mask.value_or(0xFFFF));
    });

    // Raycast all - multiple hits
    physics.set_function("raycast_all", [](const Vec3& origin, const Vec3& direction,
                                           float max_distance, sol::optional<uint16_t> layer_mask) -> std::vector<RaycastHit> {
        auto* world = get_script_context().physics_world;
        if (!world) {
            core::log(core::LogLevel::Warn, "Physics.raycast_all called without physics context");
            return {};
        }
        return world->raycast_all(origin, direction, max_distance, layer_mask.value_or(0xFFFF));
    });

    // Sphere cast
    physics.set_function("sphere_cast", [](const Vec3& origin, const Vec3& direction,
                                           float radius, float max_distance,
                                           sol::optional<uint16_t> layer_mask) -> RaycastHit {
        auto* world = get_script_context().physics_world;
        if (!world) {
            core::log(core::LogLevel::Warn, "Physics.sphere_cast called without physics context");
            return RaycastHit{};
        }
        return world->sphere_cast(origin, direction, radius, max_distance, layer_mask.value_or(0xFFFF));
    });

    // Box cast
    physics.set_function("box_cast", [](const Vec3& origin, const Vec3& direction,
                                        const Vec3& half_extents, const Quat& rotation,
                                        float max_distance, sol::optional<uint16_t> layer_mask) -> RaycastHit {
        auto* world = get_script_context().physics_world;
        if (!world) {
            core::log(core::LogLevel::Warn, "Physics.box_cast called without physics context");
            return RaycastHit{};
        }
        return world->box_cast(origin, direction, half_extents, rotation, max_distance, layer_mask.value_or(0xFFFF));
    });

    // Capsule cast
    physics.set_function("capsule_cast", [](const Vec3& origin, const Vec3& direction,
                                            float radius, float half_height, const Quat& rotation,
                                            float max_distance, sol::optional<uint16_t> layer_mask) -> RaycastHit {
        auto* world = get_script_context().physics_world;
        if (!world) {
            core::log(core::LogLevel::Warn, "Physics.capsule_cast called without physics context");
            return RaycastHit{};
        }
        return world->capsule_cast(origin, direction, radius, half_height, rotation, max_distance, layer_mask.value_or(0xFFFF));
    });

    // Overlap sphere - returns list of entity IDs
    physics.set_function("overlap_sphere", [](const Vec3& center, float radius,
                                              sol::optional<uint16_t> layer_mask) -> std::vector<uint32_t> {
        auto* physics_world = get_script_context().physics_world;
        auto* scene_world = get_current_script_world();
        if (!physics_world || !scene_world) {
            core::log(core::LogLevel::Warn, "Physics.overlap_sphere called without context");
            return {};
        }

        auto body_ids = physics_world->overlap_sphere(center, radius, layer_mask.value_or(0xFFFF));

        // Convert PhysicsBodyIds to entity IDs
        std::vector<uint32_t> result;
        result.reserve(body_ids.size());

        auto view = scene_world->view<RigidBodyComponent>();
        for (auto entity : view) {
            auto& rb = view.get<RigidBodyComponent>(entity);
            for (const auto& body_id : body_ids) {
                if (rb.body_id.id == body_id.id) {
                    result.push_back(static_cast<uint32_t>(entity));
                    break;
                }
            }
        }
        return result;
    });

    // Overlap box - returns list of entity IDs
    physics.set_function("overlap_box", [](const Vec3& center, const Vec3& half_extents,
                                           const Quat& rotation, sol::optional<uint16_t> layer_mask) -> std::vector<uint32_t> {
        auto* physics_world = get_script_context().physics_world;
        auto* scene_world = get_current_script_world();
        if (!physics_world || !scene_world) {
            core::log(core::LogLevel::Warn, "Physics.overlap_box called without context");
            return {};
        }

        auto body_ids = physics_world->overlap_box(center, half_extents, rotation, layer_mask.value_or(0xFFFF));

        // Convert PhysicsBodyIds to entity IDs
        std::vector<uint32_t> result;
        result.reserve(body_ids.size());

        auto view = scene_world->view<RigidBodyComponent>();
        for (auto entity : view) {
            auto& rb = view.get<RigidBodyComponent>(entity);
            for (const auto& body_id : body_ids) {
                if (rb.body_id.id == body_id.id) {
                    result.push_back(static_cast<uint32_t>(entity));
                    break;
                }
            }
        }
        return result;
    });

    // Apply force to entity's rigid body
    physics.set_function("add_force", [](uint32_t entity_id, const Vec3& force) {
        auto* physics_world = get_script_context().physics_world;
        auto* scene_world = get_current_script_world();
        if (!physics_world || !scene_world) {
            core::log(core::LogLevel::Warn, "Physics.add_force called without context");
            return;
        }

        auto entity = static_cast<entt::entity>(entity_id);
        if (!scene_world->registry().valid(entity)) return;

        auto* rb = scene_world->try_get<RigidBodyComponent>(entity);
        if (rb && rb->body_id.valid()) {
            physics_world->add_force(rb->body_id, force);
        }
    });

    // Apply force at a point
    physics.set_function("add_force_at_point", [](uint32_t entity_id, const Vec3& force, const Vec3& point) {
        auto* physics_world = get_script_context().physics_world;
        auto* scene_world = get_current_script_world();
        if (!physics_world || !scene_world) {
            core::log(core::LogLevel::Warn, "Physics.add_force_at_point called without context");
            return;
        }

        auto entity = static_cast<entt::entity>(entity_id);
        if (!scene_world->registry().valid(entity)) return;

        auto* rb = scene_world->try_get<RigidBodyComponent>(entity);
        if (rb && rb->body_id.valid()) {
            physics_world->add_force_at_point(rb->body_id, force, point);
        }
    });

    // Apply impulse to entity's rigid body
    physics.set_function("add_impulse", [](uint32_t entity_id, const Vec3& impulse) {
        auto* physics_world = get_script_context().physics_world;
        auto* scene_world = get_current_script_world();
        if (!physics_world || !scene_world) {
            core::log(core::LogLevel::Warn, "Physics.add_impulse called without context");
            return;
        }

        auto entity = static_cast<entt::entity>(entity_id);
        if (!scene_world->registry().valid(entity)) return;

        auto* rb = scene_world->try_get<RigidBodyComponent>(entity);
        if (rb && rb->body_id.valid()) {
            physics_world->add_impulse(rb->body_id, impulse);
        }
    });

    // Apply impulse at a point
    physics.set_function("add_impulse_at_point", [](uint32_t entity_id, const Vec3& impulse, const Vec3& point) {
        auto* physics_world = get_script_context().physics_world;
        auto* scene_world = get_current_script_world();
        if (!physics_world || !scene_world) {
            core::log(core::LogLevel::Warn, "Physics.add_impulse_at_point called without context");
            return;
        }

        auto entity = static_cast<entt::entity>(entity_id);
        if (!scene_world->registry().valid(entity)) return;

        auto* rb = scene_world->try_get<RigidBodyComponent>(entity);
        if (rb && rb->body_id.valid()) {
            physics_world->add_impulse_at_point(rb->body_id, impulse, point);
        }
    });

    // Apply torque
    physics.set_function("add_torque", [](uint32_t entity_id, const Vec3& torque) {
        auto* physics_world = get_script_context().physics_world;
        auto* scene_world = get_current_script_world();
        if (!physics_world || !scene_world) {
            core::log(core::LogLevel::Warn, "Physics.add_torque called without context");
            return;
        }

        auto entity = static_cast<entt::entity>(entity_id);
        if (!scene_world->registry().valid(entity)) return;

        auto* rb = scene_world->try_get<RigidBodyComponent>(entity);
        if (rb && rb->body_id.valid()) {
            physics_world->add_torque(rb->body_id, torque);
        }
    });

    // Get linear velocity
    physics.set_function("get_velocity", [](uint32_t entity_id) -> Vec3 {
        auto* physics_world = get_script_context().physics_world;
        auto* scene_world = get_current_script_world();
        if (!physics_world || !scene_world) {
            return Vec3{0.0f};
        }

        auto entity = static_cast<entt::entity>(entity_id);
        if (!scene_world->registry().valid(entity)) return Vec3{0.0f};

        auto* rb = scene_world->try_get<RigidBodyComponent>(entity);
        if (rb && rb->body_id.valid()) {
            return physics_world->get_linear_velocity(rb->body_id);
        }
        return Vec3{0.0f};
    });

    // Set linear velocity
    physics.set_function("set_velocity", [](uint32_t entity_id, const Vec3& velocity) {
        auto* physics_world = get_script_context().physics_world;
        auto* scene_world = get_current_script_world();
        if (!physics_world || !scene_world) {
            return;
        }

        auto entity = static_cast<entt::entity>(entity_id);
        if (!scene_world->registry().valid(entity)) return;

        auto* rb = scene_world->try_get<RigidBodyComponent>(entity);
        if (rb && rb->body_id.valid()) {
            physics_world->set_linear_velocity(rb->body_id, velocity);
        }
    });

    // Get angular velocity
    physics.set_function("get_angular_velocity", [](uint32_t entity_id) -> Vec3 {
        auto* physics_world = get_script_context().physics_world;
        auto* scene_world = get_current_script_world();
        if (!physics_world || !scene_world) {
            return Vec3{0.0f};
        }

        auto entity = static_cast<entt::entity>(entity_id);
        if (!scene_world->registry().valid(entity)) return Vec3{0.0f};

        auto* rb = scene_world->try_get<RigidBodyComponent>(entity);
        if (rb && rb->body_id.valid()) {
            return physics_world->get_angular_velocity(rb->body_id);
        }
        return Vec3{0.0f};
    });

    // Set angular velocity
    physics.set_function("set_angular_velocity", [](uint32_t entity_id, const Vec3& velocity) {
        auto* physics_world = get_script_context().physics_world;
        auto* scene_world = get_current_script_world();
        if (!physics_world || !scene_world) {
            return;
        }

        auto entity = static_cast<entt::entity>(entity_id);
        if (!scene_world->registry().valid(entity)) return;

        auto* rb = scene_world->try_get<RigidBodyComponent>(entity);
        if (rb && rb->body_id.valid()) {
            physics_world->set_angular_velocity(rb->body_id, velocity);
        }
    });

    // Get gravity
    physics.set_function("get_gravity", []() -> Vec3 {
        auto* world = get_script_context().physics_world;
        if (!world) {
            return Vec3{0.0f, -9.81f, 0.0f};
        }
        return world->get_gravity();
    });

    // Common layer constants
    physics["LAYER_DEFAULT"] = static_cast<uint16_t>(1 << 0);
    physics["LAYER_STATIC"] = static_cast<uint16_t>(1 << 1);
    physics["LAYER_DYNAMIC"] = static_cast<uint16_t>(1 << 2);
    physics["LAYER_PLAYER"] = static_cast<uint16_t>(1 << 3);
    physics["LAYER_ENEMY"] = static_cast<uint16_t>(1 << 4);
    physics["LAYER_TRIGGER"] = static_cast<uint16_t>(1 << 5);
    physics["LAYER_ALL"] = static_cast<uint16_t>(0xFFFF);
}

} // namespace engine::script
