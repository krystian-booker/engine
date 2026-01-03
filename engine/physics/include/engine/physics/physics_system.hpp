#pragma once

#include <engine/physics/physics_world.hpp>
#include <engine/physics/character_controller.hpp>
#include <engine/physics/ragdoll.hpp>
#include <engine/scene/world.hpp>
#include <functional>

namespace engine::physics {

// PhysicsSystem wraps PhysicsWorld for ECS system registration
// Games create an instance and register its member functions with the scheduler
class PhysicsSystem {
public:
    explicit PhysicsSystem(PhysicsWorld& world);
    ~PhysicsSystem() = default;

    // Non-copyable, movable
    PhysicsSystem(const PhysicsSystem&) = delete;
    PhysicsSystem& operator=(const PhysicsSystem&) = delete;
    PhysicsSystem(PhysicsSystem&&) = default;
    PhysicsSystem& operator=(PhysicsSystem&&) = default;

    // Get the underlying physics world
    PhysicsWorld& get_world() { return *m_physics_world; }
    const PhysicsWorld& get_world() const { return *m_physics_world; }

    // Physics step - call in FixedUpdate phase (before character/ragdoll updates)
    void step(scene::World& world, double dt);

    // Update all character controllers - call in FixedUpdate phase (after physics step)
    void update_character_controllers(scene::World& world, double dt);

    // Update all ragdolls - call in FixedUpdate or Update phase
    void update_ragdolls(scene::World& world, double dt);

    // Create bound system functions for scheduler registration
    // These return std::function that can be added to SystemScheduler
    std::function<void(scene::World&, double)> create_step_system();
    std::function<void(scene::World&, double)> create_character_system();
    std::function<void(scene::World&, double)> create_ragdoll_system();

private:
    PhysicsWorld* m_physics_world;
};

} // namespace engine::physics
