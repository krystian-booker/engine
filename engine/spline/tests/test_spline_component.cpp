#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/spline/spline_component.hpp>

using namespace engine::spline;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

TEST_CASE("SplineComponent default values", "[spline][component]") {
    SplineComponent comp;

    REQUIRE(comp.mode == SplineMode::CatmullRom);
    REQUIRE(comp.end_mode == SplineEndMode::Clamp);
    REQUIRE(comp.points.empty());
    REQUIRE_THAT(comp.catmull_rom_alpha, WithinAbs(0.5f, 0.001f));
    REQUIRE(comp.auto_tangents == true);
    REQUIRE_THAT(comp.tension, WithinAbs(0.5f, 0.001f));
    REQUIRE(comp.visible == true);
    REQUIRE(comp.show_points == true);
    REQUIRE(comp.show_tangents == false);
    REQUIRE_THAT(comp.line_width, WithinAbs(2.0f, 0.001f));
    REQUIRE(comp.tessellation == 20);
}

TEST_CASE("SplineComponent color", "[spline][component]") {
    SplineComponent comp;

    // Default orange color
    REQUIRE_THAT(comp.color.x, WithinAbs(1.0f, 0.001f));  // R
    REQUIRE_THAT(comp.color.y, WithinAbs(0.5f, 0.001f));  // G
    REQUIRE_THAT(comp.color.z, WithinAbs(0.0f, 0.001f));  // B
    REQUIRE_THAT(comp.color.w, WithinAbs(1.0f, 0.001f));  // A
}

TEST_CASE("SplineComponent point storage", "[spline][component]") {
    SplineComponent comp;

    comp.points.push_back(SplinePoint(Vec3{0.0f, 0.0f, 0.0f}));
    comp.points.push_back(SplinePoint(Vec3{5.0f, 5.0f, 0.0f}));
    comp.points.push_back(SplinePoint(Vec3{10.0f, 0.0f, 0.0f}));

    REQUIRE(comp.points.size() == 3);
    REQUIRE_THAT(comp.points[1].position.x, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(comp.points[1].position.y, WithinAbs(5.0f, 0.001f));
}

TEST_CASE("SplineComponent get_spline runtime creation", "[spline][component]") {
    SplineComponent comp;
    comp.mode = SplineMode::CatmullRom;

    comp.points.push_back(SplinePoint(Vec3{0.0f, 0.0f, 0.0f}));
    comp.points.push_back(SplinePoint(Vec3{5.0f, 5.0f, 0.0f}));
    comp.points.push_back(SplinePoint(Vec3{10.0f, 0.0f, 0.0f}));
    comp.points.push_back(SplinePoint(Vec3{15.0f, 5.0f, 0.0f}));

    SECTION("Creates runtime spline on first access") {
        Spline* spline = comp.get_spline();
        REQUIRE(spline != nullptr);
        REQUIRE(spline->mode() == SplineMode::CatmullRom);
        REQUIRE(spline->point_count() == 4);
    }

    SECTION("Caches runtime spline") {
        Spline* spline1 = comp.get_spline();
        Spline* spline2 = comp.get_spline();
        REQUIRE(spline1 == spline2); // Same pointer
    }
}

TEST_CASE("SplineComponent invalidate", "[spline][component]") {
    SplineComponent comp;
    comp.points.push_back(SplinePoint(Vec3{0.0f, 0.0f, 0.0f}));
    comp.points.push_back(SplinePoint(Vec3{10.0f, 0.0f, 0.0f}));

    // Create runtime spline
    Spline* spline1 = comp.get_spline();
    REQUIRE(spline1 != nullptr);

    // Invalidate
    comp.invalidate();

    // Next access should create new spline
    Spline* spline2 = comp.get_spline();
    REQUIRE(spline2 != nullptr);
    // Note: Pointer may or may not be different depending on implementation
}

TEST_CASE("SplineComponent evaluate helpers", "[spline][component]") {
    SplineComponent comp;
    comp.mode = SplineMode::CatmullRom;

    comp.points.push_back(SplinePoint(Vec3{0.0f, 0.0f, 0.0f}));
    comp.points.push_back(SplinePoint(Vec3{5.0f, 0.0f, 0.0f}));
    comp.points.push_back(SplinePoint(Vec3{10.0f, 0.0f, 0.0f}));
    comp.points.push_back(SplinePoint(Vec3{15.0f, 0.0f, 0.0f}));

    SECTION("Evaluate position") {
        Vec3 pos = comp.evaluate_position(0.0f);
        REQUIRE_THAT(pos.x, WithinAbs(0.0f, 0.1f));
    }

    SECTION("Evaluate full result") {
        SplineEvalResult result = comp.evaluate(0.5f);
        REQUIRE(result.position.x > 0.0f);
        REQUIRE(result.position.x < 15.0f);
    }

    SECTION("Get length") {
        float length = comp.get_length();
        REQUIRE(length > 0.0f);
    }
}

TEST_CASE("SplineDebugRenderComponent defaults", "[spline][component]") {
    SplineDebugRenderComponent debug;

    REQUIRE(debug.enabled == true);
    REQUIRE(debug.render_curve == true);
    REQUIRE(debug.render_points == true);
    REQUIRE(debug.render_tangents == false);
    REQUIRE(debug.render_normals == false);
    REQUIRE(debug.render_bounds == false);
    REQUIRE_THAT(debug.point_size, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(debug.tangent_scale, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("SplineDebugRenderComponent colors", "[spline][component]") {
    SplineDebugRenderComponent debug;

    // Curve color (orange)
    REQUIRE_THAT(debug.curve_color.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(debug.curve_color.y, WithinAbs(0.5f, 0.001f));

    // Point color (yellow)
    REQUIRE_THAT(debug.point_color.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(debug.point_color.y, WithinAbs(1.0f, 0.001f));

    // Tangent color (green)
    REQUIRE_THAT(debug.tangent_color.y, WithinAbs(1.0f, 0.001f));

    // Normal color (blue-ish)
    REQUIRE_THAT(debug.normal_color.z, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("SplineEventComponent", "[spline][component]") {
    SplineEventComponent events;

    SECTION("Default empty") {
        REQUIRE(events.distance_events.empty());
        REQUIRE(events.point_events.empty());
    }

    SECTION("Add distance event") {
        SplineEventComponent::DistanceEvent de;
        de.distance = 10.0f;
        de.event_name = "halfway";
        de.triggered = false;
        de.repeat_on_loop = true;

        events.distance_events.push_back(de);

        REQUIRE(events.distance_events.size() == 1);
        REQUIRE(events.distance_events[0].event_name == "halfway");
    }

    SECTION("Add point event") {
        SplineEventComponent::PointEvent pe;
        pe.point_index = 2;
        pe.event_name = "reached_checkpoint";
        pe.triggered = false;

        events.point_events.push_back(pe);

        REQUIRE(events.point_events.size() == 1);
        REQUIRE(events.point_events[0].point_index == 2);
    }

    SECTION("Reset triggers") {
        SplineEventComponent::DistanceEvent de;
        de.triggered = true;
        events.distance_events.push_back(de);

        SplineEventComponent::PointEvent pe;
        pe.triggered = true;
        events.point_events.push_back(pe);

        events.reset_triggers();

        REQUIRE(events.distance_events[0].triggered == false);
        REQUIRE(events.point_events[0].triggered == false);
    }
}

TEST_CASE("SplineMeshComponent defaults", "[spline][component]") {
    SplineMeshComponent mesh;

    REQUIRE(mesh.profile_type == SplineMeshComponent::ProfileType::Circle);
    REQUIRE_THAT(mesh.radius, WithinAbs(0.5f, 0.001f));
    REQUIRE(mesh.radial_segments == 8);
    REQUIRE_THAT(mesh.rect_size.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(mesh.rect_size.y, WithinAbs(0.5f, 0.001f));
    REQUIRE(mesh.custom_profile.empty());
    REQUIRE(mesh.segments_per_unit == 2);
    REQUIRE(mesh.cap_start == true);
    REQUIRE(mesh.cap_end == true);
    REQUIRE(mesh.follow_spline_roll == true);
    REQUIRE_THAT(mesh.uv_scale_u, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(mesh.uv_scale_v, WithinAbs(1.0f, 0.001f));
    REQUIRE(mesh.use_scale_curve == false);
    REQUIRE(mesh.scale_curve.empty());
}

TEST_CASE("SplineMeshComponent profile types", "[spline][component]") {
    SplineMeshComponent mesh;

    SECTION("Circle profile") {
        mesh.profile_type = SplineMeshComponent::ProfileType::Circle;
        mesh.radius = 2.0f;
        mesh.radial_segments = 16;

        REQUIRE(mesh.profile_type == SplineMeshComponent::ProfileType::Circle);
        REQUIRE_THAT(mesh.radius, WithinAbs(2.0f, 0.001f));
    }

    SECTION("Rectangle profile") {
        mesh.profile_type = SplineMeshComponent::ProfileType::Rectangle;
        mesh.rect_size = Vec2{2.0f, 1.0f};

        REQUIRE(mesh.profile_type == SplineMeshComponent::ProfileType::Rectangle);
        REQUIRE_THAT(mesh.rect_size.x, WithinAbs(2.0f, 0.001f));
    }

    SECTION("Custom profile") {
        mesh.profile_type = SplineMeshComponent::ProfileType::Custom;
        mesh.custom_profile.push_back(Vec2{0.0f, 0.0f});
        mesh.custom_profile.push_back(Vec2{1.0f, 0.0f});
        mesh.custom_profile.push_back(Vec2{0.5f, 1.0f});

        REQUIRE(mesh.profile_type == SplineMeshComponent::ProfileType::Custom);
        REQUIRE(mesh.custom_profile.size() == 3);
    }
}
