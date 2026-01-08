#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/physics/physics_world.hpp>

using namespace engine::physics;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

TEST_CASE("RaycastHit defaults", "[physics][world]") {
    RaycastHit hit;

    REQUIRE_FALSE(hit.body.valid());
    REQUIRE_THAT(hit.point.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(hit.normal.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(hit.distance, WithinAbs(0.0f, 0.001f));
    REQUIRE(hit.hit == false);
}

TEST_CASE("ConstraintId", "[physics][world]") {
    SECTION("Default is invalid") {
        ConstraintId id;
        REQUIRE_FALSE(id.valid());
        REQUIRE(id.id == UINT32_MAX);
    }

    SECTION("Valid ID") {
        ConstraintId id;
        id.id = 10;
        REQUIRE(id.valid());
    }
}

TEST_CASE("FixedConstraintSettings", "[physics][world]") {
    FixedConstraintSettings settings;

    REQUIRE_FALSE(settings.body_a.valid());
    REQUIRE_FALSE(settings.body_b.valid());
    REQUIRE_THAT(settings.local_anchor_a.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(settings.local_anchor_b.x, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("HingeConstraintSettings", "[physics][world]") {
    HingeConstraintSettings settings;

    REQUIRE_FALSE(settings.body_a.valid());
    REQUIRE_FALSE(settings.body_b.valid());
    REQUIRE_THAT(settings.local_anchor_a.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(settings.hinge_axis.y, WithinAbs(1.0f, 0.001f)); // Default Y-axis
    REQUIRE_THAT(settings.limit_min, WithinAbs(-3.14159f, 0.01f));
    REQUIRE_THAT(settings.limit_max, WithinAbs(3.14159f, 0.01f));
    REQUIRE(settings.enable_limits == true);
}

TEST_CASE("SwingTwistConstraintSettings", "[physics][world]") {
    SwingTwistConstraintSettings settings;

    REQUIRE_FALSE(settings.body_a.valid());
    REQUIRE_FALSE(settings.body_b.valid());
    REQUIRE_THAT(settings.twist_axis.y, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(settings.plane_axis.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(settings.swing_limit_y, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(settings.swing_limit_z, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(settings.twist_min, WithinAbs(-0.5f, 0.001f));
    REQUIRE_THAT(settings.twist_max, WithinAbs(0.5f, 0.001f));
}

TEST_CASE("BodyShapeInfo defaults", "[physics][world]") {
    BodyShapeInfo info;

    REQUIRE(info.type == ShapeType::Box);
    REQUIRE_THAT(info.dimensions.x, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(info.center_offset.x, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("ContactPointInfo", "[physics][world]") {
    ContactPointInfo info;
    info.position = Vec3{1.0f, 2.0f, 3.0f};
    info.normal = Vec3{0.0f, 1.0f, 0.0f};
    info.penetration_depth = 0.05f;
    info.body_a.id = 1;
    info.body_b.id = 2;

    REQUIRE_THAT(info.position.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(info.normal.y, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(info.penetration_depth, WithinAbs(0.05f, 0.001f));
    REQUIRE(info.body_a.id == 1);
    REQUIRE(info.body_b.id == 2);
}

TEST_CASE("ConstraintInfo", "[physics][world]") {
    ConstraintInfo info;
    info.id.id = 5;
    info.body_a.id = 1;
    info.body_b.id = 2;
    info.world_anchor_a = Vec3{0.0f, 1.0f, 0.0f};
    info.world_anchor_b = Vec3{0.0f, -1.0f, 0.0f};

    REQUIRE(info.id.valid());
    REQUIRE(info.body_a.id == 1);
    REQUIRE(info.body_b.id == 2);
    REQUIRE_THAT(info.world_anchor_a.y, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(info.world_anchor_b.y, WithinAbs(-1.0f, 0.001f));
}

// Note: PhysicsWorld requires initialization with Jolt, which needs proper setup.
// These tests cover the data structures. Integration tests would require a full
// physics world initialization with the engine's PhysicsSettings.
