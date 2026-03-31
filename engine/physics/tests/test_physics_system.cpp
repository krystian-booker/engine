#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/physics/physics_system.hpp>
#include <engine/scene/scene_serializer.hpp>
#include <engine/scene/transform.hpp>

using namespace engine::physics;
using namespace engine::scene;
using Catch::Matchers::WithinAbs;

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

TEST_CASE("PhysicsSystem uses world-space for parented rigid bodies and syncs back to local space", "[physics][system][hierarchy]") {
    engine::core::PhysicsSettings settings;
    PhysicsWorld physics;
    physics.init(settings);

    World world;
    PhysicsSystem system{physics};
    system.attach_world(world);

    Entity parent = world.create("Parent");
    auto& parent_transform = world.emplace<LocalTransform>(parent);
    parent_transform.position = Vec3{10.0f, 0.0f, 0.0f};

    Entity child = world.create("Child");
    auto& child_transform = world.emplace<LocalTransform>(child);
    child_transform.position = Vec3{1.0f, 0.0f, 0.0f};
    world.emplace<RigidBodyComponent>(child, BoxShapeSettings{Vec3{0.5f}}).set_type(BodyType::Kinematic);
    set_parent(world, child, parent);

    transform_system(world, settings.fixed_timestep);
    system.prepare_rigid_bodies(world, settings.fixed_timestep);

    auto& rigid_body = world.get<RigidBodyComponent>(child);
    REQUIRE(rigid_body.initialized);
    REQUIRE_THAT(physics.get_position(rigid_body.body_id).x, WithinAbs(11.0f, 0.001f));

    physics.set_transform(rigid_body.body_id, Vec3{15.0f, 0.0f, 0.0f}, Quat{1.0f, 0.0f, 0.0f, 0.0f});
    system.update_rigid_bodies(world, settings.fixed_timestep);

    REQUIRE_THAT(world.get<LocalTransform>(child).position.x, WithinAbs(5.0f, 0.001f));
    transform_system(world, settings.fixed_timestep);
    REQUIRE_THAT(world.get<WorldTransform>(child).position().x, WithinAbs(15.0f, 0.001f));
}

TEST_CASE("SceneSerializer round-trips rigid body authoring data", "[physics][serializer]") {
    World source_world;
    SceneSerializer serializer;

    Entity entity = source_world.create("Body");
    source_world.emplace<LocalTransform>(entity, Vec3{0.0f, 1.0f, 0.0f});

    auto& rigid_body = source_world.emplace<RigidBodyComponent>(entity, SphereShapeSettings{2.5f});
    rigid_body.type = BodyType::Kinematic;
    rigid_body.mass = 3.0f;
    rigid_body.friction = 0.25f;
    rigid_body.restitution = 0.6f;
    rigid_body.layer = layers::TRIGGER;
    rigid_body.is_sensor = true;
    rigid_body.allow_sleep = false;

    const std::string json = serializer.serialize(source_world);

    World loaded_world;
    REQUIRE(serializer.deserialize(loaded_world, json));

    Entity loaded = loaded_world.find_by_name("Body");
    REQUIRE(loaded != NullEntity);
    REQUIRE(loaded_world.has<RigidBodyComponent>(loaded));

    const auto& loaded_rb = loaded_world.get<RigidBodyComponent>(loaded);
    REQUIRE(loaded_rb.type == BodyType::Kinematic);
    REQUIRE_THAT(loaded_rb.mass, WithinAbs(3.0f, 0.001f));
    REQUIRE_THAT(loaded_rb.friction, WithinAbs(0.25f, 0.001f));
    REQUIRE_THAT(loaded_rb.restitution, WithinAbs(0.6f, 0.001f));
    REQUIRE(loaded_rb.layer == layers::TRIGGER);
    REQUIRE(loaded_rb.is_sensor);
    REQUIRE_FALSE(loaded_rb.allow_sleep);

    const auto* sphere = std::get_if<SphereShapeSettings>(&loaded_rb.shape);
    REQUIRE(sphere != nullptr);
    REQUIRE_THAT(sphere->radius, WithinAbs(2.5f, 0.001f));
}
