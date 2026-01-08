#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/physics/body.hpp>

using namespace engine::physics;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

TEST_CASE("PhysicsBodyId", "[physics][body]") {
    SECTION("Default is invalid") {
        PhysicsBodyId id;
        REQUIRE_FALSE(id.valid());
        REQUIRE(id.id == UINT32_MAX);
    }

    SECTION("Valid ID") {
        PhysicsBodyId id;
        id.id = 42;
        REQUIRE(id.valid());
    }
}

TEST_CASE("BodyType enum", "[physics][body]") {
    REQUIRE(static_cast<uint8_t>(BodyType::Static) == 0);
    REQUIRE(static_cast<uint8_t>(BodyType::Kinematic) == 1);
    REQUIRE(static_cast<uint8_t>(BodyType::Dynamic) == 2);
}

TEST_CASE("BodySettings defaults", "[physics][body]") {
    BodySettings settings;

    REQUIRE(settings.type == BodyType::Dynamic);
    REQUIRE(settings.shape == nullptr);

    REQUIRE_THAT(settings.position.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(settings.position.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(settings.position.z, WithinAbs(0.0f, 0.001f));

    REQUIRE_THAT(settings.rotation.w, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(settings.rotation.x, WithinAbs(0.0f, 0.001f));

    REQUIRE_THAT(settings.linear_velocity.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(settings.angular_velocity.x, WithinAbs(0.0f, 0.001f));

    REQUIRE_THAT(settings.mass, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(settings.friction, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(settings.restitution, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(settings.linear_damping, WithinAbs(0.05f, 0.001f));
    REQUIRE_THAT(settings.angular_damping, WithinAbs(0.05f, 0.001f));

    REQUIRE(settings.layer == layers::DYNAMIC);
    REQUIRE(settings.is_sensor == false);
    REQUIRE(settings.allow_sleep == true);

    REQUIRE(settings.lock_rotation_x == false);
    REQUIRE(settings.lock_rotation_y == false);
    REQUIRE(settings.lock_rotation_z == false);

    REQUIRE(settings.user_data == nullptr);
}

TEST_CASE("BodySettings custom values", "[physics][body]") {
    BoxShapeSettings box{Vec3{1.0f}};
    BodySettings settings;

    settings.type = BodyType::Static;
    settings.shape = &box;
    settings.position = Vec3{10.0f, 20.0f, 30.0f};
    settings.mass = 5.0f;
    settings.friction = 0.8f;
    settings.restitution = 0.5f;
    settings.layer = layers::STATIC;
    settings.is_sensor = true;
    settings.lock_rotation_y = true;

    REQUIRE(settings.type == BodyType::Static);
    REQUIRE(settings.shape == &box);
    REQUIRE_THAT(settings.position.x, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(settings.mass, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(settings.friction, WithinAbs(0.8f, 0.001f));
    REQUIRE_THAT(settings.restitution, WithinAbs(0.5f, 0.001f));
    REQUIRE(settings.layer == layers::STATIC);
    REQUIRE(settings.is_sensor == true);
    REQUIRE(settings.lock_rotation_y == true);
}

TEST_CASE("ContactPoint structure", "[physics][body]") {
    ContactPoint contact;
    contact.position = Vec3{1.0f, 2.0f, 3.0f};
    contact.normal = Vec3{0.0f, 1.0f, 0.0f};
    contact.penetration_depth = 0.01f;
    contact.impulse = Vec3{0.0f, 10.0f, 0.0f};

    REQUIRE_THAT(contact.position.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(contact.normal.y, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(contact.penetration_depth, WithinAbs(0.01f, 0.001f));
    REQUIRE_THAT(contact.impulse.y, WithinAbs(10.0f, 0.001f));
}

TEST_CASE("CollisionEvent structure", "[physics][body]") {
    CollisionEvent event;
    event.body_a.id = 1;
    event.body_b.id = 2;
    event.contact.position = Vec3{5.0f, 0.0f, 5.0f};
    event.contact.normal = Vec3{0.0f, 1.0f, 0.0f};
    event.is_start = true;

    REQUIRE(event.body_a.id == 1);
    REQUIRE(event.body_b.id == 2);
    REQUIRE_THAT(event.contact.position.x, WithinAbs(5.0f, 0.001f));
    REQUIRE(event.is_start == true);
}
