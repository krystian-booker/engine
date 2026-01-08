#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/physics/shapes.hpp>

using namespace engine::physics;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

TEST_CASE("ShapeType enum", "[physics][shapes]") {
    REQUIRE(static_cast<uint8_t>(ShapeType::Box) == 0);
    REQUIRE(static_cast<uint8_t>(ShapeType::Sphere) == 1);
    REQUIRE(static_cast<uint8_t>(ShapeType::Capsule) == 2);
    REQUIRE(static_cast<uint8_t>(ShapeType::Cylinder) == 3);
    REQUIRE(static_cast<uint8_t>(ShapeType::ConvexHull) == 4);
    REQUIRE(static_cast<uint8_t>(ShapeType::Mesh) == 5);
    REQUIRE(static_cast<uint8_t>(ShapeType::HeightField) == 6);
    REQUIRE(static_cast<uint8_t>(ShapeType::Compound) == 7);
}

TEST_CASE("ShapeSettings base defaults", "[physics][shapes]") {
    ShapeSettings settings;

    REQUIRE(settings.type == ShapeType::Box);
    REQUIRE_THAT(settings.center_offset.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(settings.center_offset.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(settings.center_offset.z, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(settings.rotation_offset.w, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("BoxShapeSettings", "[physics][shapes]") {
    SECTION("Default construction") {
        BoxShapeSettings box;
        REQUIRE(box.type == ShapeType::Box);
        REQUIRE_THAT(box.half_extents.x, WithinAbs(0.5f, 0.001f));
        REQUIRE_THAT(box.half_extents.y, WithinAbs(0.5f, 0.001f));
        REQUIRE_THAT(box.half_extents.z, WithinAbs(0.5f, 0.001f));
    }

    SECTION("Construction with extents") {
        BoxShapeSettings box{Vec3{1.0f, 2.0f, 3.0f}};
        REQUIRE(box.type == ShapeType::Box);
        REQUIRE_THAT(box.half_extents.x, WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(box.half_extents.y, WithinAbs(2.0f, 0.001f));
        REQUIRE_THAT(box.half_extents.z, WithinAbs(3.0f, 0.001f));
    }
}

TEST_CASE("SphereShapeSettings", "[physics][shapes]") {
    SECTION("Default construction") {
        SphereShapeSettings sphere;
        REQUIRE(sphere.type == ShapeType::Sphere);
        REQUIRE_THAT(sphere.radius, WithinAbs(0.5f, 0.001f));
    }

    SECTION("Construction with radius") {
        SphereShapeSettings sphere{2.5f};
        REQUIRE(sphere.type == ShapeType::Sphere);
        REQUIRE_THAT(sphere.radius, WithinAbs(2.5f, 0.001f));
    }
}

TEST_CASE("CapsuleShapeSettings", "[physics][shapes]") {
    SECTION("Default construction") {
        CapsuleShapeSettings capsule;
        REQUIRE(capsule.type == ShapeType::Capsule);
        REQUIRE_THAT(capsule.radius, WithinAbs(0.5f, 0.001f));
        REQUIRE_THAT(capsule.half_height, WithinAbs(0.5f, 0.001f));
    }

    SECTION("Construction with parameters") {
        CapsuleShapeSettings capsule{0.3f, 1.0f};
        REQUIRE(capsule.type == ShapeType::Capsule);
        REQUIRE_THAT(capsule.radius, WithinAbs(0.3f, 0.001f));
        REQUIRE_THAT(capsule.half_height, WithinAbs(1.0f, 0.001f));
    }
}

TEST_CASE("CylinderShapeSettings", "[physics][shapes]") {
    SECTION("Default construction") {
        CylinderShapeSettings cylinder;
        REQUIRE(cylinder.type == ShapeType::Cylinder);
        REQUIRE_THAT(cylinder.radius, WithinAbs(0.5f, 0.001f));
        REQUIRE_THAT(cylinder.half_height, WithinAbs(0.5f, 0.001f));
    }

    SECTION("Construction with parameters") {
        CylinderShapeSettings cylinder{1.0f, 2.0f};
        REQUIRE(cylinder.type == ShapeType::Cylinder);
        REQUIRE_THAT(cylinder.radius, WithinAbs(1.0f, 0.001f));
        REQUIRE_THAT(cylinder.half_height, WithinAbs(2.0f, 0.001f));
    }
}

TEST_CASE("ConvexHullShapeSettings", "[physics][shapes]") {
    ConvexHullShapeSettings hull;

    REQUIRE(hull.type == ShapeType::ConvexHull);
    REQUIRE(hull.points.empty());

    hull.points.push_back(Vec3{0.0f, 0.0f, 0.0f});
    hull.points.push_back(Vec3{1.0f, 0.0f, 0.0f});
    hull.points.push_back(Vec3{0.5f, 1.0f, 0.0f});

    REQUIRE(hull.points.size() == 3);
}

TEST_CASE("MeshShapeSettings", "[physics][shapes]") {
    MeshShapeSettings mesh;

    REQUIRE(mesh.type == ShapeType::Mesh);
    REQUIRE(mesh.vertices.empty());
    REQUIRE(mesh.indices.empty());

    // Simple triangle
    mesh.vertices.push_back(Vec3{0.0f, 0.0f, 0.0f});
    mesh.vertices.push_back(Vec3{1.0f, 0.0f, 0.0f});
    mesh.vertices.push_back(Vec3{0.5f, 0.0f, 1.0f});
    mesh.indices.push_back(0);
    mesh.indices.push_back(1);
    mesh.indices.push_back(2);

    REQUIRE(mesh.vertices.size() == 3);
    REQUIRE(mesh.indices.size() == 3);
}

TEST_CASE("HeightFieldShapeSettings", "[physics][shapes]") {
    HeightFieldShapeSettings heightfield;

    REQUIRE(heightfield.type == ShapeType::HeightField);
    REQUIRE(heightfield.heights.empty());
    REQUIRE(heightfield.num_rows == 0);
    REQUIRE(heightfield.num_cols == 0);
    REQUIRE_THAT(heightfield.scale.x, WithinAbs(1.0f, 0.001f));

    // Simple 3x3 heightfield
    heightfield.num_rows = 3;
    heightfield.num_cols = 3;
    heightfield.heights = {
        0.0f, 0.5f, 0.0f,
        0.5f, 1.0f, 0.5f,
        0.0f, 0.5f, 0.0f
    };

    REQUIRE(heightfield.heights.size() == 9);
}

TEST_CASE("CompoundShapeSettings", "[physics][shapes]") {
    CompoundShapeSettings compound;

    REQUIRE(compound.type == ShapeType::Compound);
    REQUIRE(compound.children.empty());

    // Add child shapes
    BoxShapeSettings box{Vec3{0.5f}};
    SphereShapeSettings sphere{0.3f};

    CompoundShapeSettings::Child child1;
    child1.shape = &box;
    child1.position = Vec3{1.0f, 0.0f, 0.0f};

    CompoundShapeSettings::Child child2;
    child2.shape = &sphere;
    child2.position = Vec3{-1.0f, 0.0f, 0.0f};

    compound.children.push_back(child1);
    compound.children.push_back(child2);

    REQUIRE(compound.children.size() == 2);
    REQUIRE_THAT(compound.children[0].position.x, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("ShapeVariant", "[physics][shapes]") {
    SECTION("Box variant") {
        ShapeVariant shape = BoxShapeSettings{Vec3{1.0f, 2.0f, 3.0f}};
        REQUIRE(std::holds_alternative<BoxShapeSettings>(shape));

        auto& box = std::get<BoxShapeSettings>(shape);
        REQUIRE_THAT(box.half_extents.x, WithinAbs(1.0f, 0.001f));
    }

    SECTION("Sphere variant") {
        ShapeVariant shape = SphereShapeSettings{2.0f};
        REQUIRE(std::holds_alternative<SphereShapeSettings>(shape));

        auto& sphere = std::get<SphereShapeSettings>(shape);
        REQUIRE_THAT(sphere.radius, WithinAbs(2.0f, 0.001f));
    }

    SECTION("Capsule variant") {
        ShapeVariant shape = CapsuleShapeSettings{0.5f, 1.0f};
        REQUIRE(std::holds_alternative<CapsuleShapeSettings>(shape));
    }
}
