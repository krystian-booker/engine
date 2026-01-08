#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/spline/catmull_rom.hpp>

using namespace engine::spline;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

TEST_CASE("CatmullRomSpline construction", "[spline][catmullrom]") {
    CatmullRomSpline spline;

    REQUIRE(spline.mode() == SplineMode::CatmullRom);
    REQUIRE(spline.point_count() == 0);
    REQUIRE_THAT(spline.alpha, WithinAbs(0.5f, 0.001f)); // Default centripetal
}

TEST_CASE("CatmullRomSpline alpha parameter", "[spline][catmullrom]") {
    CatmullRomSpline spline;

    SECTION("Uniform (alpha = 0)") {
        spline.alpha = 0.0f;
        REQUIRE_THAT(spline.alpha, WithinAbs(0.0f, 0.001f));
    }

    SECTION("Centripetal (alpha = 0.5)") {
        spline.alpha = 0.5f;
        REQUIRE_THAT(spline.alpha, WithinAbs(0.5f, 0.001f));
    }

    SECTION("Chordal (alpha = 1.0)") {
        spline.alpha = 1.0f;
        REQUIRE_THAT(spline.alpha, WithinAbs(1.0f, 0.001f));
    }
}

TEST_CASE("CatmullRomSpline point management", "[spline][catmullrom]") {
    CatmullRomSpline spline;

    SECTION("Add points") {
        spline.add_point(SplinePoint(Vec3{0.0f, 0.0f, 0.0f}));
        spline.add_point(SplinePoint(Vec3{1.0f, 1.0f, 0.0f}));
        spline.add_point(SplinePoint(Vec3{2.0f, 0.0f, 0.0f}));
        spline.add_point(SplinePoint(Vec3{3.0f, 1.0f, 0.0f}));

        REQUIRE(spline.point_count() == 4);
    }

    SECTION("Get point") {
        spline.add_point(SplinePoint(Vec3{5.0f, 10.0f, 15.0f}));

        const auto& point = spline.get_point(0);
        REQUIRE_THAT(point.position.x, WithinAbs(5.0f, 0.001f));
        REQUIRE_THAT(point.position.y, WithinAbs(10.0f, 0.001f));
        REQUIRE_THAT(point.position.z, WithinAbs(15.0f, 0.001f));
    }

    SECTION("Clear points") {
        spline.add_point(SplinePoint(Vec3{0.0f, 0.0f, 0.0f}));
        spline.add_point(SplinePoint(Vec3{1.0f, 0.0f, 0.0f}));

        spline.clear_points();

        REQUIRE(spline.point_count() == 0);
    }
}

TEST_CASE("CatmullRomSpline evaluation", "[spline][catmullrom]") {
    CatmullRomSpline spline;

    // Create a simple path
    spline.add_point(SplinePoint(Vec3{0.0f, 0.0f, 0.0f}));
    spline.add_point(SplinePoint(Vec3{5.0f, 0.0f, 0.0f}));
    spline.add_point(SplinePoint(Vec3{10.0f, 0.0f, 0.0f}));
    spline.add_point(SplinePoint(Vec3{15.0f, 0.0f, 0.0f}));

    SECTION("Evaluate at start (t=0)") {
        Vec3 pos = spline.evaluate_position(0.0f);
        REQUIRE_THAT(pos.x, WithinAbs(0.0f, 0.1f));
    }

    SECTION("Evaluate at end (t=1)") {
        Vec3 pos = spline.evaluate_position(1.0f);
        REQUIRE_THAT(pos.x, WithinAbs(15.0f, 0.1f));
    }

    SECTION("Spline passes through control points") {
        // At t=0, should be at first point
        Vec3 pos0 = spline.evaluate_position(0.0f);
        REQUIRE_THAT(pos0.x, WithinAbs(0.0f, 0.1f));

        // At t=1, should be at last point
        Vec3 pos1 = spline.evaluate_position(1.0f);
        REQUIRE_THAT(pos1.x, WithinAbs(15.0f, 0.1f));
    }
}

TEST_CASE("CatmullRomSpline smooth curve property", "[spline][catmullrom]") {
    CatmullRomSpline spline;

    // Create a curved path
    spline.add_point(SplinePoint(Vec3{0.0f, 0.0f, 0.0f}));
    spline.add_point(SplinePoint(Vec3{5.0f, 5.0f, 0.0f}));
    spline.add_point(SplinePoint(Vec3{10.0f, 0.0f, 0.0f}));
    spline.add_point(SplinePoint(Vec3{15.0f, 5.0f, 0.0f}));

    SECTION("Curve is smooth (tangent continuity)") {
        // Sample multiple points and verify no sharp corners
        Vec3 prev_tangent = spline.evaluate_tangent(0.0f);

        for (float t = 0.1f; t <= 1.0f; t += 0.1f) {
            Vec3 tangent = spline.evaluate_tangent(t);
            // Tangent direction shouldn't change drastically
            float dot = glm::dot(glm::normalize(prev_tangent), glm::normalize(tangent));
            REQUIRE(dot > 0.0f); // Should generally point in similar direction
            prev_tangent = tangent;
        }
    }
}

TEST_CASE("CatmullRomSpline arc length", "[spline][catmullrom]") {
    CatmullRomSpline spline;

    // Approximately straight line
    spline.add_point(SplinePoint(Vec3{0.0f, 0.0f, 0.0f}));
    spline.add_point(SplinePoint(Vec3{5.0f, 0.0f, 0.0f}));
    spline.add_point(SplinePoint(Vec3{10.0f, 0.0f, 0.0f}));
    spline.add_point(SplinePoint(Vec3{15.0f, 0.0f, 0.0f}));

    SECTION("Total length is positive") {
        float length = spline.get_length();
        REQUIRE(length > 0.0f);
    }

    SECTION("Length increases monotonically") {
        float prev_length = 0.0f;
        for (float t = 0.1f; t <= 1.0f; t += 0.1f) {
            float length = spline.get_length_to(t);
            REQUIRE(length >= prev_length);
            prev_length = length;
        }
    }
}

TEST_CASE("CatmullRomSpline get tangent at point", "[spline][catmullrom]") {
    CatmullRomSpline spline;

    spline.add_point(SplinePoint(Vec3{0.0f, 0.0f, 0.0f}));
    spline.add_point(SplinePoint(Vec3{5.0f, 5.0f, 0.0f}));
    spline.add_point(SplinePoint(Vec3{10.0f, 0.0f, 0.0f}));

    SECTION("Get tangent at middle point") {
        Vec3 tangent = spline.get_tangent_at_point(1);
        // Tangent should be computed automatically
        REQUIRE(glm::length(tangent) > 0.0f);
    }
}

TEST_CASE("CatmullRomSpline tessellation", "[spline][catmullrom]") {
    CatmullRomSpline spline;

    spline.add_point(SplinePoint(Vec3{0.0f, 0.0f, 0.0f}));
    spline.add_point(SplinePoint(Vec3{5.0f, 5.0f, 0.0f}));
    spline.add_point(SplinePoint(Vec3{10.0f, 0.0f, 0.0f}));
    spline.add_point(SplinePoint(Vec3{15.0f, 5.0f, 0.0f}));

    auto points = spline.tessellate(10);

    REQUIRE(points.size() > 0);
    REQUIRE_THAT(points.front().x, WithinAbs(0.0f, 0.1f));
    REQUIRE_THAT(points.back().x, WithinAbs(15.0f, 0.1f));
}

TEST_CASE("CatmullRomSpline nearest point", "[spline][catmullrom]") {
    CatmullRomSpline spline;

    spline.add_point(SplinePoint(Vec3{0.0f, 0.0f, 0.0f}));
    spline.add_point(SplinePoint(Vec3{10.0f, 0.0f, 0.0f}));
    spline.add_point(SplinePoint(Vec3{20.0f, 0.0f, 0.0f}));
    spline.add_point(SplinePoint(Vec3{30.0f, 0.0f, 0.0f}));

    SECTION("Find nearest point on spline") {
        Vec3 query{15.0f, 5.0f, 0.0f}; // Above the spline
        SplineNearestResult result = spline.find_nearest_point(query);

        // Nearest point should be approximately at x=15 on the spline
        REQUIRE_THAT(result.position.x, WithinAbs(15.0f, 1.0f));
        REQUIRE_THAT(result.position.y, WithinAbs(0.0f, 0.1f));
        REQUIRE_THAT(result.distance, WithinAbs(5.0f, 0.5f));
    }

    SECTION("Find nearest t") {
        Vec3 query{10.0f, 0.0f, 0.0f}; // On the spline
        float t = spline.find_nearest_t(query);

        REQUIRE(t >= 0.0f);
        REQUIRE(t <= 1.0f);
    }
}

TEST_CASE("create_smooth_path helper", "[spline][catmullrom]") {
    std::vector<Vec3> points = {
        Vec3{0.0f, 0.0f, 0.0f},
        Vec3{5.0f, 5.0f, 0.0f},
        Vec3{10.0f, 0.0f, 0.0f},
        Vec3{15.0f, 5.0f, 0.0f}
    };

    SECTION("Non-looping path") {
        CatmullRomSpline spline = create_smooth_path(points, false);

        REQUIRE(spline.point_count() == 4);
        REQUIRE(spline.end_mode == SplineEndMode::Clamp);
    }

    SECTION("Looping path") {
        CatmullRomSpline spline = create_smooth_path(points, true);

        REQUIRE(spline.point_count() == 4);
        REQUIRE(spline.end_mode == SplineEndMode::Loop);
    }
}
