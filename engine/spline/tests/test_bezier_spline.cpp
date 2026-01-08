#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/spline/bezier_spline.hpp>

using namespace engine::spline;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

TEST_CASE("BezierSpline construction", "[spline][bezier]") {
    BezierSpline spline;

    REQUIRE(spline.mode() == SplineMode::Bezier);
    REQUIRE(spline.point_count() == 0);
}

TEST_CASE("BezierSpline point management", "[spline][bezier]") {
    BezierSpline spline;

    SECTION("Add points") {
        spline.add_point(SplinePoint(Vec3{0.0f, 0.0f, 0.0f}));
        spline.add_point(SplinePoint(Vec3{1.0f, 0.0f, 0.0f}));
        spline.add_point(SplinePoint(Vec3{2.0f, 0.0f, 0.0f}));

        REQUIRE(spline.point_count() == 3);
    }

    SECTION("Get point") {
        spline.add_point(SplinePoint(Vec3{5.0f, 10.0f, 15.0f}));

        const auto& point = spline.get_point(0);
        REQUIRE_THAT(point.position.x, WithinAbs(5.0f, 0.001f));
        REQUIRE_THAT(point.position.y, WithinAbs(10.0f, 0.001f));
        REQUIRE_THAT(point.position.z, WithinAbs(15.0f, 0.001f));
    }

    SECTION("Set point") {
        spline.add_point(SplinePoint(Vec3{0.0f, 0.0f, 0.0f}));

        SplinePoint new_point(Vec3{100.0f, 200.0f, 300.0f});
        spline.set_point(0, new_point);

        const auto& point = spline.get_point(0);
        REQUIRE_THAT(point.position.x, WithinAbs(100.0f, 0.001f));
    }

    SECTION("Insert point") {
        spline.add_point(SplinePoint(Vec3{0.0f, 0.0f, 0.0f}));
        spline.add_point(SplinePoint(Vec3{2.0f, 0.0f, 0.0f}));

        spline.insert_point(1, SplinePoint(Vec3{1.0f, 0.0f, 0.0f}));

        REQUIRE(spline.point_count() == 3);
        REQUIRE_THAT(spline.get_point(1).position.x, WithinAbs(1.0f, 0.001f));
    }

    SECTION("Remove point") {
        spline.add_point(SplinePoint(Vec3{0.0f, 0.0f, 0.0f}));
        spline.add_point(SplinePoint(Vec3{1.0f, 0.0f, 0.0f}));
        spline.add_point(SplinePoint(Vec3{2.0f, 0.0f, 0.0f}));

        spline.remove_point(1);

        REQUIRE(spline.point_count() == 2);
        REQUIRE_THAT(spline.get_point(1).position.x, WithinAbs(2.0f, 0.001f));
    }

    SECTION("Clear points") {
        spline.add_point(SplinePoint(Vec3{0.0f, 0.0f, 0.0f}));
        spline.add_point(SplinePoint(Vec3{1.0f, 0.0f, 0.0f}));

        spline.clear_points();

        REQUIRE(spline.point_count() == 0);
    }
}

TEST_CASE("BezierSpline evaluation", "[spline][bezier]") {
    BezierSpline spline;

    // Create a simple horizontal line from (0,0,0) to (10,0,0)
    SplinePoint p0(Vec3{0.0f, 0.0f, 0.0f});
    p0.tangent_out = Vec3{3.0f, 0.0f, 0.0f};

    SplinePoint p1(Vec3{10.0f, 0.0f, 0.0f});
    p1.tangent_in = Vec3{-3.0f, 0.0f, 0.0f};

    spline.add_point(p0);
    spline.add_point(p1);

    SECTION("Evaluate at start (t=0)") {
        Vec3 pos = spline.evaluate_position(0.0f);
        REQUIRE_THAT(pos.x, WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(pos.y, WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(pos.z, WithinAbs(0.0f, 0.01f));
    }

    SECTION("Evaluate at end (t=1)") {
        Vec3 pos = spline.evaluate_position(1.0f);
        REQUIRE_THAT(pos.x, WithinAbs(10.0f, 0.01f));
        REQUIRE_THAT(pos.y, WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(pos.z, WithinAbs(0.0f, 0.01f));
    }

    SECTION("Evaluate at middle (t=0.5)") {
        Vec3 pos = spline.evaluate_position(0.5f);
        REQUIRE_THAT(pos.x, WithinAbs(5.0f, 0.1f)); // Approximately middle
        REQUIRE_THAT(pos.y, WithinAbs(0.0f, 0.01f));
    }

    SECTION("Full evaluation returns frame") {
        SplineEvalResult result = spline.evaluate(0.5f);
        REQUIRE_THAT(result.position.x, WithinAbs(5.0f, 0.1f));
        // Tangent should point in +X direction
        REQUIRE(result.tangent.x > 0.5f);
    }
}

TEST_CASE("BezierSpline tangent evaluation", "[spline][bezier]") {
    BezierSpline spline;

    // Create a horizontal line
    SplinePoint p0(Vec3{0.0f, 0.0f, 0.0f});
    p0.tangent_out = Vec3{1.0f, 0.0f, 0.0f};

    SplinePoint p1(Vec3{10.0f, 0.0f, 0.0f});
    p1.tangent_in = Vec3{-1.0f, 0.0f, 0.0f};

    spline.add_point(p0);
    spline.add_point(p1);

    Vec3 tangent = spline.evaluate_tangent(0.5f);

    // Tangent should point in +X direction
    REQUIRE(tangent.x > 0.0f);
    REQUIRE_THAT(tangent.y, WithinAbs(0.0f, 0.01f));
    REQUIRE_THAT(tangent.z, WithinAbs(0.0f, 0.01f));
}

TEST_CASE("BezierSpline arc length", "[spline][bezier]") {
    BezierSpline spline;

    // Simple straight line from (0,0,0) to (10,0,0)
    SplinePoint p0(Vec3{0.0f, 0.0f, 0.0f});
    p0.tangent_out = Vec3{3.33f, 0.0f, 0.0f};

    SplinePoint p1(Vec3{10.0f, 0.0f, 0.0f});
    p1.tangent_in = Vec3{-3.33f, 0.0f, 0.0f};

    spline.add_point(p0);
    spline.add_point(p1);

    SECTION("Total length approximately 10") {
        float length = spline.get_length();
        REQUIRE_THAT(length, WithinAbs(10.0f, 0.5f));
    }

    SECTION("Length to t=0.5 approximately half") {
        float full_length = spline.get_length();
        float half_length = spline.get_length_to(0.5f);
        REQUIRE_THAT(half_length, WithinAbs(full_length * 0.5f, 0.5f));
    }
}

TEST_CASE("BezierSpline arc-length parameterization", "[spline][bezier]") {
    BezierSpline spline;

    SplinePoint p0(Vec3{0.0f, 0.0f, 0.0f});
    p0.tangent_out = Vec3{3.0f, 0.0f, 0.0f};

    SplinePoint p1(Vec3{10.0f, 0.0f, 0.0f});
    p1.tangent_in = Vec3{-3.0f, 0.0f, 0.0f};

    spline.add_point(p0);
    spline.add_point(p1);

    SECTION("Get t at distance 0") {
        float t = spline.get_t_at_distance(0.0f);
        REQUIRE_THAT(t, WithinAbs(0.0f, 0.01f));
    }

    SECTION("Get t at full length") {
        float length = spline.get_length();
        float t = spline.get_t_at_distance(length);
        REQUIRE_THAT(t, WithinAbs(1.0f, 0.01f));
    }

    SECTION("Evaluate at distance") {
        SplineEvalResult result = spline.evaluate_at_distance(0.0f);
        REQUIRE_THAT(result.position.x, WithinAbs(0.0f, 0.1f));
    }
}

TEST_CASE("BezierSpline tessellation", "[spline][bezier]") {
    BezierSpline spline;

    spline.add_point(SplinePoint(Vec3{0.0f, 0.0f, 0.0f}));
    spline.add_point(SplinePoint(Vec3{10.0f, 0.0f, 0.0f}));

    SECTION("Tessellate with 10 subdivisions") {
        auto points = spline.tessellate(10);
        REQUIRE(points.size() == 11); // 10 subdivisions = 11 points
    }

    SECTION("Tessellate with 5 subdivisions") {
        auto points = spline.tessellate(5);
        REQUIRE(points.size() == 6);
    }

    SECTION("First and last tessellation points match endpoints") {
        auto points = spline.tessellate(10);
        REQUIRE_THAT(points.front().x, WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(points.back().x, WithinAbs(10.0f, 0.01f));
    }
}

TEST_CASE("BezierSpline bounding box", "[spline][bezier]") {
    BezierSpline spline;

    spline.add_point(SplinePoint(Vec3{0.0f, 0.0f, 0.0f}));
    spline.add_point(SplinePoint(Vec3{10.0f, 5.0f, 3.0f}));

    AABB bounds = spline.get_bounds();

    REQUIRE(bounds.min.x <= 0.0f);
    REQUIRE(bounds.min.y <= 0.0f);
    REQUIRE(bounds.min.z <= 0.0f);
    REQUIRE(bounds.max.x >= 10.0f);
    REQUIRE(bounds.max.y >= 5.0f);
    REQUIRE(bounds.max.z >= 3.0f);
}

TEST_CASE("BezierSpline continuity operations", "[spline][bezier]") {
    BezierSpline spline;

    SplinePoint p0(Vec3{0.0f, 0.0f, 0.0f});
    p0.tangent_in = Vec3{-1.0f, 0.0f, 0.0f};
    p0.tangent_out = Vec3{1.0f, 0.0f, 0.0f};

    SplinePoint p1(Vec3{5.0f, 0.0f, 0.0f});
    p1.tangent_in = Vec3{-1.0f, 1.0f, 0.0f};
    p1.tangent_out = Vec3{1.0f, 0.0f, 0.0f};

    SplinePoint p2(Vec3{10.0f, 0.0f, 0.0f});
    p2.tangent_in = Vec3{-1.0f, 0.0f, 0.0f};
    p2.tangent_out = Vec3{1.0f, 0.0f, 0.0f};

    spline.add_point(p0);
    spline.add_point(p1);
    spline.add_point(p2);

    SECTION("Make smooth at point") {
        spline.make_smooth(1);
        // After make_smooth, tangent_in and tangent_out should be aligned
        const auto& point = spline.get_point(1);
        Vec3 dir_in = glm::normalize(-point.tangent_in);
        Vec3 dir_out = glm::normalize(point.tangent_out);
        float dot = glm::dot(dir_in, dir_out);
        REQUIRE_THAT(dot, WithinAbs(1.0f, 0.01f));
    }
}

TEST_CASE("Create bezier from path helper", "[spline][bezier]") {
    std::vector<Vec3> path = {
        Vec3{0.0f, 0.0f, 0.0f},
        Vec3{5.0f, 5.0f, 0.0f},
        Vec3{10.0f, 0.0f, 0.0f}
    };

    BezierSpline spline = create_bezier_from_path(path, 0.3f);

    REQUIRE(spline.point_count() == 3);
    REQUIRE_THAT(spline.get_point(0).position.x, WithinAbs(0.0f, 0.01f));
    REQUIRE_THAT(spline.get_point(2).position.x, WithinAbs(10.0f, 0.01f));
}

TEST_CASE("Create bezier circle helper", "[spline][bezier]") {
    Vec3 center{5.0f, 0.0f, 5.0f};
    float radius = 3.0f;

    BezierSpline spline = create_bezier_circle(center, radius);

    REQUIRE(spline.point_count() > 0);
    REQUIRE(spline.end_mode == SplineEndMode::Loop);

    // Points should be at radius distance from center
    for (size_t i = 0; i < spline.point_count(); ++i) {
        Vec3 to_point = spline.get_point(i).position - center;
        float dist = glm::length(to_point);
        REQUIRE_THAT(dist, WithinAbs(radius, 0.1f));
    }
}
