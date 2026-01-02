#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/core/math.hpp>

using namespace engine::core;
using Catch::Matchers::WithinAbs;

TEST_CASE("AABB construction and properties", "[core][math][aabb]") {
    SECTION("Default construction") {
        AABB aabb;
        REQUIRE(aabb.min == Vec3{0.0f});
        REQUIRE(aabb.max == Vec3{0.0f});
    }

    SECTION("Parameterized construction") {
        AABB aabb{{-1.0f, -2.0f, -3.0f}, {1.0f, 2.0f, 3.0f}};
        REQUIRE(aabb.min == Vec3{-1.0f, -2.0f, -3.0f});
        REQUIRE(aabb.max == Vec3{1.0f, 2.0f, 3.0f});
    }

    SECTION("Center calculation") {
        AABB aabb{{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}};
        Vec3 center = aabb.center();
        REQUIRE_THAT(center.x, WithinAbs(0.0f, 0.0001f));
        REQUIRE_THAT(center.y, WithinAbs(0.0f, 0.0001f));
        REQUIRE_THAT(center.z, WithinAbs(0.0f, 0.0001f));
    }

    SECTION("Size calculation") {
        AABB aabb{{0.0f, 0.0f, 0.0f}, {2.0f, 4.0f, 6.0f}};
        Vec3 size = aabb.size();
        REQUIRE_THAT(size.x, WithinAbs(2.0f, 0.0001f));
        REQUIRE_THAT(size.y, WithinAbs(4.0f, 0.0001f));
        REQUIRE_THAT(size.z, WithinAbs(6.0f, 0.0001f));
    }

    SECTION("Extents calculation") {
        AABB aabb{{-1.0f, -2.0f, -3.0f}, {1.0f, 2.0f, 3.0f}};
        Vec3 extents = aabb.extents();
        REQUIRE_THAT(extents.x, WithinAbs(1.0f, 0.0001f));
        REQUIRE_THAT(extents.y, WithinAbs(2.0f, 0.0001f));
        REQUIRE_THAT(extents.z, WithinAbs(3.0f, 0.0001f));
    }
}

TEST_CASE("AABB containment tests", "[core][math][aabb]") {
    AABB aabb{{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}};

    SECTION("Point at center is contained") {
        REQUIRE(aabb.contains({0.0f, 0.0f, 0.0f}));
    }

    SECTION("Point at corner is contained") {
        REQUIRE(aabb.contains({1.0f, 1.0f, 1.0f}));
        REQUIRE(aabb.contains({-1.0f, -1.0f, -1.0f}));
    }

    SECTION("Point outside is not contained") {
        REQUIRE_FALSE(aabb.contains({2.0f, 0.0f, 0.0f}));
        REQUIRE_FALSE(aabb.contains({0.0f, -2.0f, 0.0f}));
    }
}

TEST_CASE("AABB intersection tests", "[core][math][aabb]") {
    AABB aabb1{{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}};

    SECTION("Overlapping AABBs intersect") {
        AABB aabb2{{0.0f, 0.0f, 0.0f}, {2.0f, 2.0f, 2.0f}};
        REQUIRE(aabb1.intersects(aabb2));
        REQUIRE(aabb2.intersects(aabb1));
    }

    SECTION("Touching AABBs intersect") {
        AABB aabb2{{1.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 1.0f}};
        REQUIRE(aabb1.intersects(aabb2));
    }

    SECTION("Separated AABBs do not intersect") {
        AABB aabb2{{5.0f, 5.0f, 5.0f}, {6.0f, 6.0f, 6.0f}};
        REQUIRE_FALSE(aabb1.intersects(aabb2));
    }
}

TEST_CASE("AABB expansion", "[core][math][aabb]") {
    SECTION("Expand by point") {
        AABB aabb{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}};
        aabb.expand({2.0f, -1.0f, 0.5f});
        REQUIRE(aabb.min == Vec3{0.0f, -1.0f, 0.0f});
        REQUIRE(aabb.max == Vec3{2.0f, 1.0f, 1.0f});
    }

    SECTION("Expand by AABB") {
        AABB aabb1{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}};
        AABB aabb2{{-1.0f, 0.5f, 0.0f}, {0.5f, 2.0f, 0.5f}};
        aabb1.expand(aabb2);
        REQUIRE(aabb1.min == Vec3{-1.0f, 0.0f, 0.0f});
        REQUIRE(aabb1.max == Vec3{1.0f, 2.0f, 1.0f});
    }
}

TEST_CASE("Ray construction and evaluation", "[core][math][ray]") {
    SECTION("Default construction") {
        Ray ray;
        REQUIRE(ray.origin == Vec3{0.0f});
        REQUIRE(ray.direction == Vec3{0.0f, 0.0f, -1.0f});
    }

    SECTION("Parameterized construction normalizes direction") {
        Ray ray{{1.0f, 2.0f, 3.0f}, {2.0f, 0.0f, 0.0f}};
        REQUIRE(ray.origin == Vec3{1.0f, 2.0f, 3.0f});
        REQUIRE_THAT(ray.direction.x, WithinAbs(1.0f, 0.0001f));
        REQUIRE_THAT(ray.direction.y, WithinAbs(0.0f, 0.0001f));
        REQUIRE_THAT(ray.direction.z, WithinAbs(0.0f, 0.0001f));
    }

    SECTION("Point along ray") {
        Ray ray{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}};
        Vec3 point = ray.at(5.0f);
        REQUIRE_THAT(point.x, WithinAbs(5.0f, 0.0001f));
        REQUIRE_THAT(point.y, WithinAbs(0.0f, 0.0001f));
        REQUIRE_THAT(point.z, WithinAbs(0.0f, 0.0001f));
    }
}
