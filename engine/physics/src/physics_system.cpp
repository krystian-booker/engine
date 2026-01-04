#include <engine/physics/physics_system.hpp>
#include <engine/physics/rigid_body_component.hpp>
#include <engine/physics/water_volume.hpp>
#include <engine/physics/buoyancy_component.hpp>
#include <engine/physics/boat.hpp>
#include <engine/physics/vehicle.hpp>
#include <engine/physics/cloth.hpp>
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

void PhysicsSystem::update_rigid_bodies(scene::World& world, double dt) {
    if (!m_physics_world) return;

    rigid_body_sync_system(world, *m_physics_world, static_cast<float>(dt));
}

void PhysicsSystem::update_water_volumes(scene::World& world, double dt) {
    // Update water volumes with wave animations
    auto view = world.view<WaterVolumeControllerComponent, scene::LocalTransform>();

    for (auto entity : view) {
        auto& wvc = view.get<WaterVolumeControllerComponent>(entity);
        auto& transform = view.get<scene::LocalTransform>(entity);

        if (!wvc.volume) continue;

        // Sync water volume position with transform
        wvc.volume->set_position(transform.position);
        wvc.volume->set_rotation(transform.rotation);

        // Update wave animation
        wvc.volume->update(static_cast<float>(dt));
    }

    // Also update the global water volume manager
    get_water_volumes().update_all(static_cast<float>(dt));
}

void PhysicsSystem::update_buoyancy(scene::World& world, double dt) {
    if (!m_physics_world) return;

    // Find all water volumes
    auto water_view = world.view<WaterVolumeControllerComponent, scene::LocalTransform>();

    // Process all entities with buoyancy and rigid body components
    auto buoyancy_view = world.view<BuoyancyComponent, RigidBodyComponent, scene::LocalTransform>();

    for (auto entity : buoyancy_view) {
        auto& bc = buoyancy_view.get<BuoyancyComponent>(entity);
        auto& rb = buoyancy_view.get<RigidBodyComponent>(entity);
        auto& transform = buoyancy_view.get<scene::LocalTransform>(entity);

        if (!rb.initialized || !rb.body_id.valid()) continue;

        // Track previous water state for events
        bool was_in_water = bc.is_in_water;

        // Find water volume at entity position
        WaterVolume* water = get_water_volumes().find_volume_at(transform.position);

        if (water) {
            bc.is_in_water = true;

            // Calculate surface height at entity position
            float surface_height = water->get_surface_height_at(transform.position);
            float water_density = water->get_density();

            // Apply buoyancy force
            float buoyancy_force = m_physics_world->apply_buoyancy(
                rb.body_id,
                surface_height,
                water_density,
                bc.buoyancy_multiplier
            );

            // Calculate submerged fraction for drag
            float submerged_volume = m_physics_world->calculate_submerged_volume(rb.body_id, surface_height);
            float total_volume = m_physics_world->get_body_volume(rb.body_id);
            bc.submerged_fraction = (total_volume > 0.0f) ? (submerged_volume / total_volume) : 0.0f;

            // Apply water drag
            m_physics_world->apply_water_drag(
                rb.body_id,
                bc.submerged_fraction,
                water->get_linear_drag() * bc.water_drag_multiplier,
                water->get_angular_drag() * bc.water_drag_multiplier
            );

            // Store forces for debug
            bc.last_buoyancy_force = Vec3{0.0f, buoyancy_force, 0.0f};
        } else {
            bc.is_in_water = false;
            bc.submerged_fraction = 0.0f;
        }

        // Update events
        bc.just_entered_water = (!was_in_water && bc.is_in_water);
        bc.just_exited_water = (was_in_water && !bc.is_in_water);
    }
}

void PhysicsSystem::update_boats(scene::World& world, double dt) {
    if (!m_physics_world) return;

    auto view = world.view<BoatControllerComponent, scene::LocalTransform>();

    for (auto entity : view) {
        auto& bc = view.get<BoatControllerComponent>(entity);
        auto& transform = view.get<scene::LocalTransform>(entity);

        if (!bc.boat) continue;
        if (!bc.boat->is_initialized()) continue;

        // Find water volume at boat position
        WaterVolume* water = get_water_volumes().find_volume_at(transform.position);

        // Update boat physics
        bc.boat->update(static_cast<float>(dt), water);

        // Sync boat transform back to ECS
        transform.position = bc.boat->get_position();
        transform.rotation = bc.boat->get_rotation();
    }
}

void PhysicsSystem::update_vehicles(scene::World& world, double dt) {
    if (!m_physics_world) return;

    auto view = world.view<VehicleControllerComponent, scene::LocalTransform>();

    for (auto entity : view) {
        auto& vc = view.get<VehicleControllerComponent>(entity);
        auto& transform = view.get<scene::LocalTransform>(entity);

        if (!vc.vehicle) continue;
        if (!vc.vehicle->is_initialized()) continue;
        if (!vc.vehicle->is_enabled()) continue;

        // Update vehicle physics
        vc.vehicle->update(static_cast<float>(dt));

        // Sync vehicle transform back to ECS
        transform.position = vc.vehicle->get_position();
        transform.rotation = vc.vehicle->get_rotation();
    }
}

void PhysicsSystem::update_cloth(scene::World& world, double dt) {
    if (!m_physics_world) return;

    auto view = world.view<ClothControllerComponent, scene::LocalTransform>();

    for (auto entity : view) {
        auto& cc = view.get<ClothControllerComponent>(entity);
        auto& transform = view.get<scene::LocalTransform>(entity);

        if (!cc.cloth) continue;
        if (!cc.cloth->is_initialized()) continue;
        if (!cc.cloth->is_enabled()) continue;

        // Sync cloth position with entity transform
        cc.cloth->set_position(transform.position);
        cc.cloth->set_rotation(transform.rotation);

        // Update cloth physics
        cc.cloth->update(static_cast<float>(dt));
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

std::function<void(scene::World&, double)> PhysicsSystem::create_rigid_body_system() {
    return [this](scene::World& world, double dt) {
        this->update_rigid_bodies(world, dt);
    };
}

std::function<void(scene::World&, double)> PhysicsSystem::create_water_volume_system() {
    return [this](scene::World& world, double dt) {
        this->update_water_volumes(world, dt);
    };
}

std::function<void(scene::World&, double)> PhysicsSystem::create_buoyancy_system() {
    return [this](scene::World& world, double dt) {
        this->update_buoyancy(world, dt);
    };
}

std::function<void(scene::World&, double)> PhysicsSystem::create_boat_system() {
    return [this](scene::World& world, double dt) {
        this->update_boats(world, dt);
    };
}

std::function<void(scene::World&, double)> PhysicsSystem::create_vehicle_system() {
    return [this](scene::World& world, double dt) {
        this->update_vehicles(world, dt);
    };
}

std::function<void(scene::World&, double)> PhysicsSystem::create_cloth_system() {
    return [this](scene::World& world, double dt) {
        this->update_cloth(world, dt);
    };
}

} // namespace engine::physics
