#include <catch2/catch_test_macros.hpp>
#include <engine/physics/physics_system.hpp>
#include <engine/scene/transform.hpp>

using namespace engine::physics;
using namespace engine::scene;

TEST_CASE("PhysicsSystem cleans up rigid bodies on entity destruction", "[physics][system]") {
    engine::core::PhysicsSettings settings;
    PhysicsWorld physics;
    physics.init(settings);

    World world;
    PhysicsSystem system{physics};
    system.attach_world(world);

    Entity entity = world.create("DynamicBody");
    world.emplace<LocalTransform>(entity, Vec3{0.0f, 5.0f, 0.0f});
    world.emplace<RigidBodyComponent>(entity, BoxShapeSettings{Vec3{0.5f}});

    system.prepare_rigid_bodies(world, settings.fixed_timestep);
    REQUIRE(physics.get_body_count() == 1);

    world.destroy(entity);
    REQUIRE(physics.get_body_count() == 0);
}

TEST_CASE("PhysicsSystem cleans up rigid bodies on world clear", "[physics][system]") {
    engine::core::PhysicsSettings settings;
    PhysicsWorld physics;
    physics.init(settings);

    World world;
    PhysicsSystem system{physics};
    system.attach_world(world);

    Entity entity = world.create("ClearableBody");
    world.emplace<LocalTransform>(entity, Vec3{0.0f, 2.0f, 0.0f});
    world.emplace<RigidBodyComponent>(entity, SphereShapeSettings{0.5f});

    system.prepare_rigid_bodies(world, settings.fixed_timestep);
    REQUIRE(physics.get_body_count() == 1);

    world.clear();
    REQUIRE(physics.get_body_count() == 0);
}

TEST_CASE("PhysicsSystem removes bodies for inactive entities", "[physics][system]") {
    engine::core::PhysicsSettings settings;
    PhysicsWorld physics;
    physics.init(settings);

    World world;
    PhysicsSystem system{physics};
    system.attach_world(world);

    Entity entity = world.create("DisableBody");
    world.emplace<LocalTransform>(entity, Vec3{0.0f, 3.0f, 0.0f});
    world.emplace<RigidBodyComponent>(entity, BoxShapeSettings{Vec3{0.5f}});

    system.prepare_rigid_bodies(world, settings.fixed_timestep);
    REQUIRE(physics.get_body_count() == 1);

    world.get<EntityInfo>(entity).enabled = false;
    system.prepare_rigid_bodies(world, settings.fixed_timestep);
    REQUIRE(physics.get_body_count() == 0);
}

TEST_CASE("PhysicsSystem default scheduler registration drives simulation", "[physics][system]") {
    engine::core::PhysicsSettings settings;
    PhysicsWorld physics;
    physics.init(settings);

    World world;
    PhysicsSystem system{physics};
    system.attach_world(world);

    Scheduler scheduler;
    scheduler.add(Phase::FixedUpdate, transform_system, "transform_fixed", 10);
    system.register_default_systems(scheduler);

    Entity entity = world.create("FallingBody");
    world.emplace<LocalTransform>(entity, Vec3{0.0f, 10.0f, 0.0f});
    world.emplace<RigidBodyComponent>(entity, SphereShapeSettings{0.5f});

    scheduler.run(world, settings.fixed_timestep, Phase::FixedUpdate);

    REQUIRE(physics.get_body_count() == 1);
    REQUIRE(world.get<LocalTransform>(entity).position.y < 10.0f);
}
