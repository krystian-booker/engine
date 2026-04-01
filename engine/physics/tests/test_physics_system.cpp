#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/physics/boat.hpp>
#include <engine/physics/buoyancy_component.hpp>
#include <engine/physics/cloth.hpp>
#include <engine/physics/physics_system.hpp>
#include <engine/physics/vehicle_component.hpp>
#include <engine/physics/water_volume.hpp>
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

TEST_CASE("SceneSerializer round-trips advanced physics authoring data", "[physics][serializer]") {
    PhysicsWorld registration_world;
    PhysicsSystem registration_system{registration_world};
    (void)registration_system;

    World source_world;
    SceneSerializer serializer;

    Entity anchor = source_world.create("Anchor");
    auto& anchor_transform = source_world.emplace<LocalTransform>(anchor);
    anchor_transform.position = Vec3{4.0f, 5.0f, 6.0f};

    Entity vehicle_entity = source_world.create("Vehicle");
    auto& vehicle = source_world.emplace<VehicleComponent>(vehicle_entity);
    vehicle.chassis_shape = CapsuleShapeSettings{1.25f, 2.5f};
    vehicle.center_of_mass_offset = Vec3{0.0f, -0.4f, 0.2f};
    vehicle.wheel_collision_mask = 0x1234u;
    vehicle.wheels = {
        WheelSettings{
            Vec3{1.0f, 0.0f, 1.5f},
            Vec3{0.0f, -1.0f, 0.0f},
            Vec3{0.0f, 1.0f, 0.0f},
            0.45f,
            0.25f,
            0.1f,
            0.45f,
            60000.0f,
            900.0f,
            0.15f,
            1.1f,
            1.2f,
            0.55f,
            true,
            true,
            false,
            2
        }
    };
    vehicle.anti_roll_bars = {
        AntiRollBarSettings{0, 1, 3450.0f}
    };
    vehicle.arcade.max_speed = 44.0f;
    vehicle.simulation.gear_ratios = {-3.2f, 3.1f, 2.0f, 1.4f};

    Entity water_entity = source_world.create("Water");
    auto& water = source_world.emplace<WaterVolumeComponent>(water_entity);
    water.shape = WaterShape::Infinite;
    water.surface_drag = 3.5f;
    water.waves.enabled = true;
    water.waves.use_gerstner = true;
    water.waves.direction = {0.25f, 0.75f};
    water.waves.steepness = 0.8f;
    water.deep_color = Vec3{0.01f, 0.02f, 0.03f};
    water.refraction_strength = 0.9f;

    Entity buoyancy_entity = source_world.create("Buoyancy");
    auto& buoyancy = source_world.emplace<BuoyancyComponent>(buoyancy_entity);
    buoyancy.mode = BuoyancyMode::Voxel;
    buoyancy.buoyancy_points = {
        BuoyancyPoint{Vec3{0.0f, -0.5f, 0.0f}, 0.35f, 1.75f}
    };
    buoyancy.voxel_resolution = Vec3{0.25f, 0.5f, 0.75f};
    buoyancy.max_voxels = 128;
    buoyancy.volume_override = 12.0f;
    buoyancy.surface_splash_threshold = 6.5f;
    buoyancy.surface_exit_threshold = 2.5f;

    Entity boat_entity = source_world.create("Boat");
    auto& boat = source_world.emplace<BoatComponent>(boat_entity);
    boat.mode = BoatMode::Simulation;
    boat.hull.hull_shape = BoxShapeSettings{Vec3{2.0f, 0.75f, 5.0f}};
    boat.hull.hull_mass = 2500.0f;
    boat.hull.buoyancy_points = {
        BuoyancyPoint{Vec3{0.0f, -0.25f, 1.5f}, 0.5f, 2.0f}
    };
    boat.propellers = {
        PropellerSettings{Vec3{0.0f, -0.5f, -2.0f}, Vec3{0.0f, 0.0f, 1.0f}, 42000.0f, 2800.0f, 0.65f, 0.82f, 0.61f, 0.35f, 0.9f}
    };
    boat.rudders = {
        RudderSettings{Vec3{0.0f, -0.2f, -2.8f}, 0.45f, 1.4f, 1.6f, 1.1f, 0.55f}
    };
    boat.arcade.wave_response = 0.75f;
    boat.collision_mask = 0x0F0Fu;
    boat.engine_on = false;

    Entity cloth_entity = source_world.create("Cloth");
    auto& cloth = source_world.emplace<ClothComponent>(cloth_entity);
    cloth.type = ClothType::Interactive;
    cloth.mesh.use_grid = false;
    cloth.mesh.vertices = {Vec3{0.0f, 0.0f, 0.0f}, Vec3{1.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f}};
    cloth.mesh.normals = {Vec3{0.0f, 0.0f, 1.0f}, Vec3{0.0f, 0.0f, 1.0f}, Vec3{0.0f, 0.0f, 1.0f}};
    cloth.mesh.uvs = {Vec2{0.0f, 0.0f}, Vec2{1.0f, 0.0f}, Vec2{0.0f, 1.0f}};
    cloth.mesh.indices = {0u, 1u, 2u};
    cloth.substep_delta = 1.0f / 240.0f;
    cloth.attachments = {
        ClothAttachment{0u, AttachmentType::Fixed, true, anchor, Vec3{0.25f, 0.5f, 0.75f}, Vec3{0.0f}, 1000.0f, 10.0f, 0.1f}
    };
    cloth.collision.self_collision = true;
    cloth.collision.collision_margin = 0.03f;
    cloth.wind_mode = ClothWindMode::Local;
    cloth.wind.direction = Vec3{0.0f, 0.0f, 1.0f};
    cloth.wind.strength = 4.0f;

    const std::string json = serializer.serialize(source_world);

    World loaded_world;
    REQUIRE(serializer.deserialize(loaded_world, json));

    const Entity loaded_anchor = loaded_world.find_by_name("Anchor");
    const Entity loaded_vehicle = loaded_world.find_by_name("Vehicle");
    const Entity loaded_water = loaded_world.find_by_name("Water");
    const Entity loaded_buoyancy = loaded_world.find_by_name("Buoyancy");
    const Entity loaded_boat = loaded_world.find_by_name("Boat");
    const Entity loaded_cloth = loaded_world.find_by_name("Cloth");

    REQUIRE(loaded_anchor != NullEntity);
    REQUIRE(loaded_vehicle != NullEntity);
    REQUIRE(loaded_water != NullEntity);
    REQUIRE(loaded_buoyancy != NullEntity);
    REQUIRE(loaded_boat != NullEntity);
    REQUIRE(loaded_cloth != NullEntity);

    const auto& loaded_vehicle_component = loaded_world.get<VehicleComponent>(loaded_vehicle);
    REQUIRE(loaded_vehicle_component.wheel_collision_mask == 0x1234u);
    REQUIRE(loaded_vehicle_component.wheels.size() == 1);
    REQUIRE_THAT(loaded_vehicle_component.wheels.front().suspension_preload, WithinAbs(0.15f, 0.001f));
    REQUIRE(loaded_vehicle_component.wheels.front().anti_roll_bar_group == 2);
    REQUIRE(loaded_vehicle_component.anti_roll_bars.size() == 1);
    REQUIRE(loaded_vehicle_component.simulation.gear_ratios.size() == 4);
    const auto* vehicle_capsule = std::get_if<CapsuleShapeSettings>(&loaded_vehicle_component.chassis_shape);
    REQUIRE(vehicle_capsule != nullptr);
    REQUIRE_THAT(vehicle_capsule->radius, WithinAbs(1.25f, 0.001f));
    REQUIRE_THAT(vehicle_capsule->half_height, WithinAbs(2.5f, 0.001f));

    const auto& loaded_water_component = loaded_world.get<WaterVolumeComponent>(loaded_water);
    REQUIRE(loaded_water_component.shape == WaterShape::Infinite);
    REQUIRE(loaded_water_component.waves.enabled);
    REQUIRE(loaded_water_component.waves.use_gerstner);
    REQUIRE_THAT(loaded_water_component.surface_drag, WithinAbs(3.5f, 0.001f));
    REQUIRE_THAT(loaded_water_component.waves.steepness, WithinAbs(0.8f, 0.001f));
    REQUIRE_THAT(loaded_water_component.refraction_strength, WithinAbs(0.9f, 0.001f));

    const auto& loaded_buoyancy_component = loaded_world.get<BuoyancyComponent>(loaded_buoyancy);
    REQUIRE(loaded_buoyancy_component.mode == BuoyancyMode::Voxel);
    REQUIRE(loaded_buoyancy_component.buoyancy_points.size() == 1);
    REQUIRE(loaded_buoyancy_component.max_voxels == 128);
    REQUIRE_THAT(loaded_buoyancy_component.volume_override, WithinAbs(12.0f, 0.001f));
    REQUIRE_THAT(loaded_buoyancy_component.surface_splash_threshold, WithinAbs(6.5f, 0.001f));

    const auto& loaded_boat_component = loaded_world.get<BoatComponent>(loaded_boat);
    REQUIRE(loaded_boat_component.mode == BoatMode::Simulation);
    REQUIRE(loaded_boat_component.propellers.size() == 1);
    REQUIRE(loaded_boat_component.rudders.size() == 1);
    REQUIRE(loaded_boat_component.hull.buoyancy_points.size() == 1);
    REQUIRE(loaded_boat_component.collision_mask == 0x0F0Fu);
    REQUIRE_FALSE(loaded_boat_component.engine_on);
    const auto* loaded_hull_box = std::get_if<BoxShapeSettings>(&loaded_boat_component.hull.hull_shape);
    REQUIRE(loaded_hull_box != nullptr);
    REQUIRE_THAT(loaded_hull_box->half_extents.z, WithinAbs(5.0f, 0.001f));

    const auto& loaded_cloth_component = loaded_world.get<ClothComponent>(loaded_cloth);
    REQUIRE(loaded_cloth_component.type == ClothType::Interactive);
    REQUIRE_FALSE(loaded_cloth_component.mesh.use_grid);
    REQUIRE(loaded_cloth_component.mesh.vertices.size() == 3);
    REQUIRE(loaded_cloth_component.attachments.size() == 1);
    REQUIRE(loaded_cloth_component.attachments.front().attach_to_entity);
    REQUIRE(loaded_cloth_component.attachments.front().entity_id == loaded_anchor);
    REQUIRE(loaded_cloth_component.wind_mode == ClothWindMode::Local);
    REQUIRE(loaded_cloth_component.collision.self_collision);
    REQUIRE_THAT(loaded_cloth_component.substep_delta, WithinAbs(1.0f / 240.0f, 0.0001f));
}

TEST_CASE("PhysicsSystem syncs cloth entity attachments from ECS anchors", "[physics][cloth][system]") {
    engine::core::PhysicsSettings settings;
    PhysicsWorld physics;
    physics.init(settings);

    World world;
    PhysicsSystem system{physics};

    Entity anchor = world.create("Anchor");
    auto& anchor_transform = world.emplace<LocalTransform>(anchor);
    anchor_transform.position = Vec3{10.0f, 0.0f, 0.0f};

    Entity cloth_entity = world.create("Cloth");
    world.emplace<LocalTransform>(cloth_entity, Vec3{0.0f, 0.0f, 0.0f});

    ClothComponent cloth_settings;
    cloth_settings.mesh.use_grid = false;
    cloth_settings.mesh.vertices = {Vec3{0.0f, 0.0f, 0.0f}};
    cloth_settings.mesh.normals = {Vec3{0.0f, 0.0f, 1.0f}};
    cloth_settings.mesh.uvs = {Vec2{0.0f, 0.0f}};
    cloth_settings.mesh.indices = {0u, 0u, 0u};
    cloth_settings.attachments = {
        ClothAttachment{0u, AttachmentType::Fixed, true, anchor, Vec3{1.0f, 2.0f, 3.0f}, Vec3{0.0f}, 1000.0f, 10.0f, 0.1f}
    };

    auto& controller = world.emplace<ClothControllerComponent>(cloth_entity);
    controller.cloth = std::make_unique<Cloth>();
    controller.cloth->init(physics, cloth_settings);

    transform_system(world, settings.fixed_timestep);
    system.update_cloth(world, settings.fixed_timestep);

    const Vec3 anchored_position = controller.cloth->get_vertex_position(0);
    REQUIRE_THAT(anchored_position.x, WithinAbs(11.0f, 0.001f));
    REQUIRE_THAT(anchored_position.y, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(anchored_position.z, WithinAbs(3.0f, 0.001f));
}
