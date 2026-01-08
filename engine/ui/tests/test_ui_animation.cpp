#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/ui/ui_animation.hpp>

using namespace engine::ui;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// ============================================================================
// EaseType enum Tests
// ============================================================================

TEST_CASE("EaseType enum", "[ui][animation]") {
    REQUIRE(static_cast<uint8_t>(EaseType::Linear) == 0);
    REQUIRE(static_cast<uint8_t>(EaseType::EaseInQuad) == 1);
    REQUIRE(static_cast<uint8_t>(EaseType::EaseOutQuad) == 2);
    REQUIRE(static_cast<uint8_t>(EaseType::EaseInOutQuad) == 3);
    REQUIRE(static_cast<uint8_t>(EaseType::EaseInCubic) == 4);
    REQUIRE(static_cast<uint8_t>(EaseType::EaseOutCubic) == 5);
    REQUIRE(static_cast<uint8_t>(EaseType::EaseInOutCubic) == 6);
    REQUIRE(static_cast<uint8_t>(EaseType::EaseInBack) == 7);
    REQUIRE(static_cast<uint8_t>(EaseType::EaseOutBack) == 8);
    REQUIRE(static_cast<uint8_t>(EaseType::EaseInOutBack) == 9);
}

// ============================================================================
// AnimProperty enum Tests
// ============================================================================

TEST_CASE("AnimProperty enum", "[ui][animation]") {
    REQUIRE(static_cast<uint8_t>(AnimProperty::Opacity) == 0);
    REQUIRE(static_cast<uint8_t>(AnimProperty::PositionX) == 1);
    REQUIRE(static_cast<uint8_t>(AnimProperty::PositionY) == 2);
    REQUIRE(static_cast<uint8_t>(AnimProperty::SizeWidth) == 3);
    REQUIRE(static_cast<uint8_t>(AnimProperty::SizeHeight) == 4);
    REQUIRE(static_cast<uint8_t>(AnimProperty::Scale) == 5);
}

// ============================================================================
// ease() function Tests
// ============================================================================

TEST_CASE("ease Linear", "[ui][animation][ease]") {
    REQUIRE_THAT(ease(EaseType::Linear, 0.0f), WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(ease(EaseType::Linear, 0.5f), WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(ease(EaseType::Linear, 1.0f), WithinAbs(1.0f, 0.001f));
}

TEST_CASE("ease EaseInQuad", "[ui][animation][ease]") {
    REQUIRE_THAT(ease(EaseType::EaseInQuad, 0.0f), WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(ease(EaseType::EaseInQuad, 1.0f), WithinAbs(1.0f, 0.001f));

    // EaseIn should be slower at start
    float mid = ease(EaseType::EaseInQuad, 0.5f);
    REQUIRE(mid < 0.5f);  // Should be less than linear
}

TEST_CASE("ease EaseOutQuad", "[ui][animation][ease]") {
    REQUIRE_THAT(ease(EaseType::EaseOutQuad, 0.0f), WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(ease(EaseType::EaseOutQuad, 1.0f), WithinAbs(1.0f, 0.001f));

    // EaseOut should be faster at start
    float mid = ease(EaseType::EaseOutQuad, 0.5f);
    REQUIRE(mid > 0.5f);  // Should be greater than linear
}

TEST_CASE("ease EaseInOutQuad", "[ui][animation][ease]") {
    REQUIRE_THAT(ease(EaseType::EaseInOutQuad, 0.0f), WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(ease(EaseType::EaseInOutQuad, 0.5f), WithinAbs(0.5f, 0.001f));  // Midpoint
    REQUIRE_THAT(ease(EaseType::EaseInOutQuad, 1.0f), WithinAbs(1.0f, 0.001f));
}

TEST_CASE("ease EaseInCubic", "[ui][animation][ease]") {
    REQUIRE_THAT(ease(EaseType::EaseInCubic, 0.0f), WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(ease(EaseType::EaseInCubic, 1.0f), WithinAbs(1.0f, 0.001f));

    // Cubic should be even slower than quad at start
    float cubic = ease(EaseType::EaseInCubic, 0.5f);
    float quad = ease(EaseType::EaseInQuad, 0.5f);
    REQUIRE(cubic < quad);
}

TEST_CASE("ease EaseOutCubic", "[ui][animation][ease]") {
    REQUIRE_THAT(ease(EaseType::EaseOutCubic, 0.0f), WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(ease(EaseType::EaseOutCubic, 1.0f), WithinAbs(1.0f, 0.001f));

    // Out cubic should be faster than out quad at start
    float cubic = ease(EaseType::EaseOutCubic, 0.5f);
    float quad = ease(EaseType::EaseOutQuad, 0.5f);
    REQUIRE(cubic > quad);
}

TEST_CASE("ease EaseInBack", "[ui][animation][ease]") {
    REQUIRE_THAT(ease(EaseType::EaseInBack, 0.0f), WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(ease(EaseType::EaseInBack, 1.0f), WithinAbs(1.0f, 0.001f));

    // EaseInBack overshoots backwards at start
    float early = ease(EaseType::EaseInBack, 0.2f);
    REQUIRE(early < 0.0f);  // Goes negative
}

TEST_CASE("ease EaseOutBack", "[ui][animation][ease]") {
    REQUIRE_THAT(ease(EaseType::EaseOutBack, 0.0f), WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(ease(EaseType::EaseOutBack, 1.0f), WithinAbs(1.0f, 0.001f));

    // EaseOutBack overshoots at end
    float late = ease(EaseType::EaseOutBack, 0.8f);
    REQUIRE(late > 1.0f);  // Overshoots past 1
}

// ============================================================================
// UITween Tests
// ============================================================================

TEST_CASE("UITween defaults", "[ui][animation][tween]") {
    UITween tween;

    REQUIRE(tween.id == 0);
    REQUIRE(tween.element == nullptr);
    REQUIRE_THAT(tween.start_value, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(tween.end_value, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(tween.duration, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(tween.elapsed, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(tween.delay, WithinAbs(0.0f, 0.001f));
    REQUIRE(tween.ease_type == EaseType::EaseOutQuad);
    REQUIRE_FALSE(tween.started);
    REQUIRE_FALSE(tween.completed);
}

TEST_CASE("UITween is_finished", "[ui][animation][tween]") {
    UITween tween;
    tween.completed = false;
    REQUIRE_FALSE(tween.is_finished());

    tween.completed = true;
    REQUIRE(tween.is_finished());
}

TEST_CASE("UITween configuration", "[ui][animation][tween]") {
    UITween tween;
    tween.id = 42;
    tween.property = AnimProperty::Opacity;
    tween.start_value = 0.0f;
    tween.end_value = 1.0f;
    tween.duration = 0.5f;
    tween.delay = 0.1f;
    tween.ease_type = EaseType::EaseInOutCubic;

    REQUIRE(tween.id == 42);
    REQUIRE(tween.property == AnimProperty::Opacity);
    REQUIRE_THAT(tween.start_value, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(tween.end_value, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(tween.duration, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(tween.delay, WithinAbs(0.1f, 0.001f));
    REQUIRE(tween.ease_type == EaseType::EaseInOutCubic);
}

TEST_CASE("UITween current_value at start", "[ui][animation][tween]") {
    UITween tween;
    tween.start_value = 0.0f;
    tween.end_value = 100.0f;
    tween.duration = 1.0f;
    tween.elapsed = 0.0f;
    tween.started = true;
    tween.ease_type = EaseType::Linear;

    float value = tween.current_value();
    REQUIRE_THAT(value, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("UITween current_value at end", "[ui][animation][tween]") {
    UITween tween;
    tween.start_value = 0.0f;
    tween.end_value = 100.0f;
    tween.duration = 1.0f;
    tween.elapsed = 1.0f;
    tween.started = true;
    tween.ease_type = EaseType::Linear;

    float value = tween.current_value();
    REQUIRE_THAT(value, WithinAbs(100.0f, 0.001f));
}

TEST_CASE("UITween current_value at midpoint linear", "[ui][animation][tween]") {
    UITween tween;
    tween.start_value = 0.0f;
    tween.end_value = 100.0f;
    tween.duration = 1.0f;
    tween.elapsed = 0.5f;
    tween.started = true;
    tween.ease_type = EaseType::Linear;

    float value = tween.current_value();
    REQUIRE_THAT(value, WithinAbs(50.0f, 0.001f));
}

// ============================================================================
// UIAnimator Tests
// ============================================================================

TEST_CASE("UIAnimator default state", "[ui][animation][animator]") {
    UIAnimator animator;

    REQUIRE(animator.active_count() == 0);
}

TEST_CASE("UIAnimator clear", "[ui][animation][animator]") {
    UIAnimator animator;
    animator.clear();

    REQUIRE(animator.active_count() == 0);
}

TEST_CASE("UIAnimator is_animating without element", "[ui][animation][animator]") {
    UIAnimator animator;

    // No element means no animations
    REQUIRE_FALSE(animator.is_animating(nullptr));
}
