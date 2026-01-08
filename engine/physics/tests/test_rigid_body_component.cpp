#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/physics/rigid_body_component.hpp>

using namespace engine::physics;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

TEST_CASE("RigidBodyComponent default construction", "[physics][component]") {
    RigidBodyComponent rb;

    REQUIRE_FALSE(rb.body_id.valid());
    REQUIRE(rb.type == BodyType::Dynamic);
    REQUIRE_THAT(rb.mass, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(rb.friction, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(rb.restitution, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(rb.linear_damping, WithinAbs(0.05f, 0.001f));
    REQUIRE_THAT(rb.angular_damping, WithinAbs(0.05f, 0.001f));
    REQUIRE(rb.layer == layers::DYNAMIC);
    REQUIRE(rb.is_sensor == false);
    REQUIRE(rb.sync_to_transform == true);
    REQUIRE(rb.allow_sleep == true);
    REQUIRE(rb.lock_rotation_x == false);
    REQUIRE(rb.lock_rotation_y == false);
    REQUIRE(rb.lock_rotation_z == false);
    REQUIRE(rb.initialized == false);

    // Default shape is a box
    REQUIRE(std::holds_alternative<BoxShapeSettings>(rb.shape));
}

TEST_CASE("RigidBodyComponent construction with shape", "[physics][component]") {
    SECTION("Sphere shape") {
        RigidBodyComponent rb{SphereShapeSettings{1.5f}};
        REQUIRE(std::holds_alternative<SphereShapeSettings>(rb.shape));

        auto& sphere = std::get<SphereShapeSettings>(rb.shape);
        REQUIRE_THAT(sphere.radius, WithinAbs(1.5f, 0.001f));
    }

    SECTION("Capsule shape") {
        RigidBodyComponent rb{CapsuleShapeSettings{0.5f, 1.0f}};
        REQUIRE(std::holds_alternative<CapsuleShapeSettings>(rb.shape));
    }
}

TEST_CASE("RigidBodyComponent fluent setters", "[physics][component]") {
    RigidBodyComponent rb;

    rb.set_type(BodyType::Kinematic)
      .set_mass(10.0f)
      .set_friction(0.8f)
      .set_restitution(0.3f)
      .set_layer(layers::PLAYER)
      .set_sensor(true)
      .set_sync(false);

    REQUIRE(rb.type == BodyType::Kinematic);
    REQUIRE_THAT(rb.mass, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(rb.friction, WithinAbs(0.8f, 0.001f));
    REQUIRE_THAT(rb.restitution, WithinAbs(0.3f, 0.001f));
    REQUIRE(rb.layer == layers::PLAYER);
    REQUIRE(rb.is_sensor == true);
    REQUIRE(rb.sync_to_transform == false);
}

TEST_CASE("RigidBodyComponent get_shape_ptr", "[physics][component]") {
    SECTION("Box shape pointer") {
        RigidBodyComponent rb{BoxShapeSettings{Vec3{1.0f, 2.0f, 3.0f}}};
        ShapeSettings* ptr = rb.get_shape_ptr();

        REQUIRE(ptr != nullptr);
        REQUIRE(ptr->type == ShapeType::Box);
    }

    SECTION("Sphere shape pointer") {
        RigidBodyComponent rb{SphereShapeSettings{2.0f}};
        ShapeSettings* ptr = rb.get_shape_ptr();

        REQUIRE(ptr != nullptr);
        REQUIRE(ptr->type == ShapeType::Sphere);
    }

    SECTION("Const get_shape_ptr") {
        const RigidBodyComponent rb{CapsuleShapeSettings{0.5f, 1.0f}};
        const ShapeSettings* ptr = rb.get_shape_ptr();

        REQUIRE(ptr != nullptr);
        REQUIRE(ptr->type == ShapeType::Capsule);
    }
}

TEST_CASE("make_static_box factory", "[physics][component]") {
    auto rb = make_static_box(Vec3{5.0f, 1.0f, 5.0f});

    REQUIRE(rb.type == BodyType::Static);
    REQUIRE(rb.layer == layers::STATIC);
    REQUIRE(std::holds_alternative<BoxShapeSettings>(rb.shape));

    auto& box = std::get<BoxShapeSettings>(rb.shape);
    REQUIRE_THAT(box.half_extents.x, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(box.half_extents.y, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("make_dynamic_box factory", "[physics][component]") {
    auto rb = make_dynamic_box(Vec3{0.5f}, 5.0f);

    REQUIRE(rb.type == BodyType::Dynamic);
    REQUIRE_THAT(rb.mass, WithinAbs(5.0f, 0.001f));
    REQUIRE(std::holds_alternative<BoxShapeSettings>(rb.shape));
}

TEST_CASE("make_dynamic_sphere factory", "[physics][component]") {
    auto rb = make_dynamic_sphere(1.0f, 2.0f);

    REQUIRE(rb.type == BodyType::Dynamic);
    REQUIRE_THAT(rb.mass, WithinAbs(2.0f, 0.001f));
    REQUIRE(std::holds_alternative<SphereShapeSettings>(rb.shape));

    auto& sphere = std::get<SphereShapeSettings>(rb.shape);
    REQUIRE_THAT(sphere.radius, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("make_trigger_box factory", "[physics][component]") {
    auto rb = make_trigger_box(Vec3{2.0f});

    REQUIRE(rb.type == BodyType::Static);
    REQUIRE(rb.is_sensor == true);
    REQUIRE(rb.layer == layers::TRIGGER);
    REQUIRE(std::holds_alternative<BoxShapeSettings>(rb.shape));
}

TEST_CASE("make_trigger_sphere factory", "[physics][component]") {
    auto rb = make_trigger_sphere(5.0f);

    REQUIRE(rb.type == BodyType::Static);
    REQUIRE(rb.is_sensor == true);
    REQUIRE(rb.layer == layers::TRIGGER);
    REQUIRE(std::holds_alternative<SphereShapeSettings>(rb.shape));

    auto& sphere = std::get<SphereShapeSettings>(rb.shape);
    REQUIRE_THAT(sphere.radius, WithinAbs(5.0f, 0.001f));
}
