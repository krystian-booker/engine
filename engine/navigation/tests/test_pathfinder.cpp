#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/navigation/pathfinder.hpp>

using namespace engine::navigation;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

TEST_CASE("PathResult defaults", "[navigation][pathfinder]") {
    PathResult result;

    REQUIRE(result.path.empty());
    REQUIRE(result.polys.empty());
    REQUIRE(result.success == false);
    REQUIRE(result.partial == false);
    REQUIRE(result.empty());
    REQUIRE(result.size() == 0);
}

TEST_CASE("PathResult with path", "[navigation][pathfinder]") {
    PathResult result;
    result.success = true;
    result.path = {
        Vec3{0.0f, 0.0f, 0.0f},
        Vec3{5.0f, 0.0f, 0.0f},
        Vec3{10.0f, 0.0f, 0.0f}
    };

    REQUIRE(result.success == true);
    REQUIRE_FALSE(result.empty());
    REQUIRE(result.size() == 3);
    REQUIRE(result.total_distance() > 0.0f);
}

TEST_CASE("PathResult partial path", "[navigation][pathfinder]") {
    PathResult result;
    result.success = true;
    result.partial = true;
    result.path = {Vec3{0.0f}, Vec3{5.0f, 0.0f, 0.0f}};

    REQUIRE(result.success == true);
    REQUIRE(result.partial == true);
}

TEST_CASE("NavRaycastResult defaults", "[navigation][pathfinder]") {
    NavRaycastResult result;

    REQUIRE(result.hit == false);
    REQUIRE_THAT(result.hit_point.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(result.hit_point.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(result.hit_point.z, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(result.hit_normal.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(result.hit_distance, WithinAbs(0.0f, 0.001f));
    REQUIRE(result.hit_poly == INVALID_NAV_POLY_REF);
}

TEST_CASE("NavRaycastResult with hit", "[navigation][pathfinder]") {
    NavRaycastResult result;
    result.hit = true;
    result.hit_point = Vec3{5.0f, 0.0f, 5.0f};
    result.hit_normal = Vec3{1.0f, 0.0f, 0.0f};
    result.hit_distance = 7.07f;
    result.hit_poly = 42;

    REQUIRE(result.hit == true);
    REQUIRE_THAT(result.hit_point.x, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(result.hit_normal.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(result.hit_distance, WithinAbs(7.07f, 0.01f));
    REQUIRE(result.hit_poly == 42);
}

TEST_CASE("NavPointResult defaults", "[navigation][pathfinder]") {
    NavPointResult result;

    REQUIRE_THAT(result.point.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(result.point.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(result.point.z, WithinAbs(0.0f, 0.001f));
    REQUIRE(result.poly == INVALID_NAV_POLY_REF);
    REQUIRE(result.valid == false);
}

TEST_CASE("NavPointResult valid point", "[navigation][pathfinder]") {
    NavPointResult result;
    result.point = Vec3{10.0f, 5.0f, 10.0f};
    result.poly = 100;
    result.valid = true;

    REQUIRE_THAT(result.point.x, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(result.point.y, WithinAbs(5.0f, 0.001f));
    REQUIRE(result.poly == 100);
    REQUIRE(result.valid == true);
}

TEST_CASE("Pathfinder default construction", "[navigation][pathfinder]") {
    Pathfinder pathfinder;

    REQUIRE_FALSE(pathfinder.is_initialized());
    REQUIRE(pathfinder.get_navmesh() == nullptr);
}

TEST_CASE("Pathfinder area costs", "[navigation][pathfinder]") {
    Pathfinder pathfinder;

    NavAreaCosts costs;
    costs.set_cost(NavAreaType::Water, 3.0f);
    costs.set_cost(NavAreaType::Road, 0.5f);

    pathfinder.set_area_costs(costs);

    const auto& retrieved = pathfinder.get_area_costs();
    REQUIRE_THAT(retrieved.get_cost(NavAreaType::Water), WithinAbs(3.0f, 0.001f));
    REQUIRE_THAT(retrieved.get_cost(NavAreaType::Road), WithinAbs(0.5f, 0.001f));
}

TEST_CASE("Pathfinder area enable/disable", "[navigation][pathfinder]") {
    Pathfinder pathfinder;

    // Areas should be enabled by default
    REQUIRE(pathfinder.is_area_enabled(NavAreaType::Walkable));
    REQUIRE(pathfinder.is_area_enabled(NavAreaType::Water));

    pathfinder.set_area_enabled(NavAreaType::Water, false);
    REQUIRE_FALSE(pathfinder.is_area_enabled(NavAreaType::Water));

    pathfinder.set_area_enabled(NavAreaType::Water, true);
    REQUIRE(pathfinder.is_area_enabled(NavAreaType::Water));
}

TEST_CASE("Pathfinder queries without initialization", "[navigation][pathfinder]") {
    Pathfinder pathfinder;

    // Queries should return safe defaults when not initialized
    auto path = pathfinder.find_path(Vec3{0.0f}, Vec3{10.0f, 0.0f, 10.0f});
    REQUIRE_FALSE(path.success);
    REQUIRE(path.empty());

    auto point = pathfinder.find_nearest_point(Vec3{0.0f});
    REQUIRE_FALSE(point.valid);

    auto raycast = pathfinder.raycast(Vec3{0.0f}, Vec3{10.0f, 0.0f, 0.0f});
    REQUIRE_FALSE(raycast.hit);

    REQUIRE_FALSE(pathfinder.is_point_on_navmesh(Vec3{0.0f}));
    REQUIRE_FALSE(pathfinder.is_path_clear(Vec3{0.0f}, Vec3{10.0f, 0.0f, 0.0f}));
    REQUIRE_FALSE(pathfinder.is_reachable(Vec3{0.0f}, Vec3{10.0f, 0.0f, 0.0f}));
}
