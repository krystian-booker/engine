#pragma once

#include <engine/physics/physics_world.hpp>
#include <engine/physics/character_controller.hpp>
#include <engine/physics/ragdoll.hpp>
#include <engine/physics/rigid_body_component.hpp>
#include <engine/scene/world.hpp>
#include <functional>

namespace engine::physics {

// Forward declarations for water physics
class WaterVolume;
struct WaterVolumeComponent;
struct BuoyancyComponent;
struct BoatComponent;

// Forward declarations for vehicle physics
class Vehicle;
struct VehicleComponent;
struct VehicleControllerComponent;

// Forward declarations for cloth physics
class Cloth;
struct ClothComponent;
struct ClothControllerComponent;

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

    // Update all rigid bodies - call in Update phase (after physics step)
    // Initializes new bodies and syncs physics transforms to ECS transforms
    void update_rigid_bodies(scene::World& world, double dt);

    // Water volume system - updates wave animations (Update phase)
    void update_water_volumes(scene::World& world, double dt);

    // Buoyancy system - applies buoyancy forces to bodies in water (FixedUpdate phase)
    void update_buoyancy(scene::World& world, double dt);

    // Boat system - updates boat physics (FixedUpdate phase, after buoyancy)
    void update_boats(scene::World& world, double dt);

    // Vehicle system - updates vehicle physics (FixedUpdate phase, after physics step)
    void update_vehicles(scene::World& world, double dt);

    // Cloth system - updates cloth/soft body physics (FixedUpdate phase)
    void update_cloth(scene::World& world, double dt);

    // Create bound system functions for scheduler registration
    // These return std::function that can be added to SystemScheduler
    std::function<void(scene::World&, double)> create_step_system();
    std::function<void(scene::World&, double)> create_character_system();
    std::function<void(scene::World&, double)> create_ragdoll_system();
    std::function<void(scene::World&, double)> create_rigid_body_system();
    std::function<void(scene::World&, double)> create_water_volume_system();
    std::function<void(scene::World&, double)> create_buoyancy_system();
    std::function<void(scene::World&, double)> create_boat_system();
    std::function<void(scene::World&, double)> create_vehicle_system();
    std::function<void(scene::World&, double)> create_cloth_system();

private:
    PhysicsWorld* m_physics_world;
};

} // namespace engine::physics
