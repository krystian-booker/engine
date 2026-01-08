#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/ui/ui_components.hpp>

using namespace engine::ui;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// ============================================================================
// UICanvasComponent Tests
// ============================================================================

TEST_CASE("UICanvasComponent defaults", "[ui][component][canvas]") {
    UICanvasComponent comp;

    REQUIRE(comp.canvas == nullptr);
    REQUIRE(comp.sort_order == 0);
    REQUIRE(comp.enabled == true);
    REQUIRE_FALSE(comp.initialized);
}

TEST_CASE("UICanvasComponent fluent API", "[ui][component][canvas]") {
    UICanvasComponent comp;

    comp.set_sort_order(10)
        .set_enabled(false);

    REQUIRE(comp.sort_order == 10);
    REQUIRE_FALSE(comp.enabled);
}

TEST_CASE("UICanvasComponent with shared canvas", "[ui][component][canvas]") {
    auto canvas = std::make_shared<UICanvas>();
    UICanvasComponent comp(canvas);

    REQUIRE(comp.canvas != nullptr);
    REQUIRE(comp.canvas.get() == canvas.get());
}

// ============================================================================
// UIWorldCanvasComponent Tests
// ============================================================================

TEST_CASE("UIWorldCanvasComponent defaults", "[ui][component][worldcanvas]") {
    UIWorldCanvasComponent comp;

    REQUIRE(comp.canvas == nullptr);
    REQUIRE_THAT(comp.offset.x, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(comp.offset.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(comp.offset.z, WithinAbs(0.0f, 0.001f));
    REQUIRE(comp.use_entity_transform == true);
    REQUIRE(comp.billboard == WorldCanvasBillboard::FaceCamera);
    REQUIRE_THAT(comp.max_distance, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(comp.fade_range, WithinAbs(10.0f, 0.001f));
    REQUIRE_FALSE(comp.constant_screen_size);
    REQUIRE_THAT(comp.reference_distance, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(comp.min_scale, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(comp.max_scale, WithinAbs(2.0f, 0.001f));
    REQUIRE(comp.enabled == true);
    REQUIRE_FALSE(comp.initialized);
}

TEST_CASE("UIWorldCanvasComponent fluent API", "[ui][component][worldcanvas]") {
    UIWorldCanvasComponent comp;

    comp.set_offset(Vec3{0.0f, 2.0f, 0.0f})
        .set_billboard(WorldCanvasBillboard::FaceCamera_Y)
        .set_max_distance(50.0f)
        .set_fade_range(5.0f)
        .set_constant_screen_size(true)
        .set_enabled(false);

    REQUIRE_THAT(comp.offset.y, WithinAbs(2.0f, 0.001f));
    REQUIRE(comp.billboard == WorldCanvasBillboard::FaceCamera_Y);
    REQUIRE_THAT(comp.max_distance, WithinAbs(50.0f, 0.001f));
    REQUIRE_THAT(comp.fade_range, WithinAbs(5.0f, 0.001f));
    REQUIRE(comp.constant_screen_size);
    REQUIRE_FALSE(comp.enabled);
}

// ============================================================================
// Factory Function Tests
// ============================================================================

TEST_CASE("make_health_bar_canvas defaults", "[ui][component][factory]") {
    UIWorldCanvasComponent comp = make_health_bar_canvas();

    REQUIRE(comp.canvas != nullptr);
    REQUIRE_THAT(comp.offset.y, WithinAbs(2.0f, 0.001f));
    REQUIRE(comp.billboard == WorldCanvasBillboard::FaceCamera);
    REQUIRE(comp.constant_screen_size == true);
    REQUIRE_THAT(comp.reference_distance, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(comp.max_distance, WithinAbs(50.0f, 0.001f));
    REQUIRE_THAT(comp.fade_range, WithinAbs(5.0f, 0.001f));
}

TEST_CASE("make_health_bar_canvas custom size", "[ui][component][factory]") {
    UIWorldCanvasComponent comp = make_health_bar_canvas(150.0f, 20.0f);

    REQUIRE(comp.canvas != nullptr);
    // Canvas should be created with custom size
}

TEST_CASE("make_nameplate_canvas defaults", "[ui][component][factory]") {
    UIWorldCanvasComponent comp = make_nameplate_canvas();

    REQUIRE(comp.canvas != nullptr);
    REQUIRE_THAT(comp.offset.y, WithinAbs(2.2f, 0.001f));  // Above health bar
    REQUIRE(comp.billboard == WorldCanvasBillboard::FaceCamera);
    REQUIRE(comp.constant_screen_size == true);
    REQUIRE_THAT(comp.reference_distance, WithinAbs(15.0f, 0.001f));
    REQUIRE_THAT(comp.max_distance, WithinAbs(30.0f, 0.001f));
}

TEST_CASE("make_interaction_prompt_canvas defaults", "[ui][component][factory]") {
    UIWorldCanvasComponent comp = make_interaction_prompt_canvas();

    REQUIRE(comp.canvas != nullptr);
    REQUIRE_THAT(comp.offset.y, WithinAbs(1.0f, 0.001f));
    REQUIRE(comp.billboard == WorldCanvasBillboard::FaceCamera);
    REQUIRE_FALSE(comp.constant_screen_size);  // Scale with distance
    REQUIRE_THAT(comp.max_distance, WithinAbs(5.0f, 0.001f));  // Only visible when close
    REQUIRE_THAT(comp.fade_range, WithinAbs(1.0f, 0.001f));
}
