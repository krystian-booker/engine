#include <engine/physics/physics_system.hpp>
#include <engine/scene/transform.hpp>

namespace engine::physics {

PhysicsSystem::PhysicsSystem(PhysicsWorld& world)
    : m_physics_world(&world)
{
}

void PhysicsSystem::step(scene::World& /*world*/, double dt) {
    if (m_physics_world) {
        m_physics_world->step(dt);
    }
}

void PhysicsSystem::update_character_controllers(scene::World& world, double dt) {
    if (!m_physics_world) return;

    auto view = world.view<CharacterControllerComponent, scene::LocalTransform>();

    for (auto entity : view) {
        auto& cc = view.get<CharacterControllerComponent>(entity);
        auto& transform = view.get<scene::LocalTransform>(entity);

        if (!cc.controller) continue;
        if (!cc.controller->is_initialized()) continue;
        if (!cc.controller->is_enabled()) continue;

        // Update the controller
        cc.controller->update(static_cast<float>(dt));

        // Sync position and rotation back to transform component
        transform.position = cc.controller->get_position();
        transform.rotation = cc.controller->get_rotation();
    }
}

void PhysicsSystem::update_ragdolls(scene::World& world, double dt) {
    if (!m_physics_world) return;

    auto view = world.view<RagdollComponent>();

    for (auto entity : view) {
        auto& rc = view.get<RagdollComponent>(entity);

        if (!rc.ragdoll) continue;
        if (!rc.ragdoll->is_initialized()) continue;

        // Update ragdoll - pass nullptr for animation pose in pure ragdoll mode
        // For powered/blending ragdolls, the game should provide animation pose
        rc.ragdoll->update(static_cast<float>(dt), nullptr);
    }
}

std::function<void(scene::World&, double)> PhysicsSystem::create_step_system() {
    return [this](scene::World& world, double dt) {
        this->step(world, dt);
    };
}

std::function<void(scene::World&, double)> PhysicsSystem::create_character_system() {
    return [this](scene::World& world, double dt) {
        this->update_character_controllers(world, dt);
    };
}

std::function<void(scene::World&, double)> PhysicsSystem::create_ragdoll_system() {
    return [this](scene::World& world, double dt) {
        this->update_ragdolls(world, dt);
    };
}

} // namespace engine::physics
