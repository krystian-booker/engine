#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/spline/spline.hpp>

using namespace engine::spline;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

TEST_CASE("SplineMode enum", "[spline]") {
    REQUIRE(static_cast<int>(SplineMode::Linear) == 0);
    REQUIRE(static_cast<int>(SplineMode::Bezier) == 1);
    REQUIRE(static_cast<int>(SplineMode::CatmullRom) == 2);
    REQUIRE(static_cast<int>(SplineMode::BSpline) == 3);
}

TEST_CASE("SplineEndMode enum", "[spline]") {
    REQUIRE(static_cast<int>(SplineEndMode::Clamp) == 0);
    REQUIRE(static_cast<int>(SplineEndMode::Loop) == 1);
    REQUIRE(static_cast<int>(SplineEndMode::PingPong) == 2);
}

TEST_CASE("SplinePoint default construction", "[spline][point]") {
    SplinePoint point;

    REQUIRE_THAT(point.position.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(point.position.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(point.position.z, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(point.tangent_in.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(point.tangent_out.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(point.roll, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(point.custom_data, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("SplinePoint construction with position", "[spline][point]") {
    SplinePoint point(Vec3{1.0f, 2.0f, 3.0f});

    REQUIRE_THAT(point.position.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(point.position.y, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(point.position.z, WithinAbs(3.0f, 0.001f));
}

TEST_CASE("SplinePoint construction with tangents", "[spline][point]") {
    Vec3 pos{1.0f, 2.0f, 3.0f};
    Vec3 tan_in{-1.0f, 0.0f, 0.0f};
    Vec3 tan_out{1.0f, 0.0f, 0.0f};

    SplinePoint point(pos, tan_in, tan_out);

    REQUIRE_THAT(point.position.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(point.tangent_in.x, WithinAbs(-1.0f, 0.001f));
    REQUIRE_THAT(point.tangent_out.x, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("SplineEvalResult defaults", "[spline][eval]") {
    SplineEvalResult result;

    REQUIRE_THAT(result.position.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(result.position.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(result.position.z, WithinAbs(0.0f, 0.001f));

    // Default tangent is forward (0, 0, 1)
    REQUIRE_THAT(result.tangent.z, WithinAbs(1.0f, 0.001f));

    // Default normal is up (0, 1, 0)
    REQUIRE_THAT(result.normal.y, WithinAbs(1.0f, 0.001f));

    // Default binormal is right (1, 0, 0)
    REQUIRE_THAT(result.binormal.x, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("SplineNearestResult defaults", "[spline][nearest]") {
    SplineNearestResult result;

    REQUIRE_THAT(result.t, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(result.position.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(result.distance, WithinAbs(0.0f, 0.001f));
    REQUIRE(result.segment_index == 0);
}

TEST_CASE("Spline factory creation", "[spline][factory]") {
    SECTION("Create Bezier spline") {
        auto spline = create_spline(SplineMode::Bezier);
        REQUIRE(spline != nullptr);
        REQUIRE(spline->mode() == SplineMode::Bezier);
    }

    SECTION("Create CatmullRom spline") {
        auto spline = create_spline(SplineMode::CatmullRom);
        REQUIRE(spline != nullptr);
        REQUIRE(spline->mode() == SplineMode::CatmullRom);
    }
}

TEST_CASE("SplineUtils make_circle", "[spline][utils]") {
    auto points = SplineUtils::make_circle(5.0f, 8);

    REQUIRE(points.size() == 8);

    // First point should be at (radius, 0, 0)
    REQUIRE_THAT(points[0].position.x, WithinAbs(5.0f, 0.01f));
    REQUIRE_THAT(points[0].position.y, WithinAbs(0.0f, 0.01f));
    REQUIRE_THAT(points[0].position.z, WithinAbs(0.0f, 0.01f));
}

TEST_CASE("SplineUtils make_helix", "[spline][utils]") {
    float radius = 2.0f;
    float height = 10.0f;
    float turns = 2.0f;
    int points_per_turn = 8;

    auto points = SplineUtils::make_helix(radius, height, turns, points_per_turn);

    REQUIRE(points.size() > 0);

    // First point should be at radius from center
    float first_dist = std::sqrt(points[0].position.x * points[0].position.x +
                                  points[0].position.z * points[0].position.z);
    REQUIRE_THAT(first_dist, WithinAbs(radius, 0.01f));

    // Last point should be at height
    REQUIRE_THAT(points.back().position.y, WithinAbs(height, 0.1f));
}

TEST_CASE("SplineUtils make_figure8", "[spline][utils]") {
    auto points = SplineUtils::make_figure8(5.0f, 16);

    REQUIRE(points.size() == 16);
}
