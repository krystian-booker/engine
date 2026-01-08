#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/cinematic/track.hpp>

using namespace engine::cinematic;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// ============================================================================
// InterpolationMode enum Tests
// ============================================================================

TEST_CASE("InterpolationMode enum", "[cinematic][track]") {
    REQUIRE(static_cast<uint8_t>(InterpolationMode::Linear) == 0);
    REQUIRE(static_cast<uint8_t>(InterpolationMode::Step) == 1);
    REQUIRE(static_cast<uint8_t>(InterpolationMode::Bezier) == 2);
    REQUIRE(static_cast<uint8_t>(InterpolationMode::CatmullRom) == 3);
}

// ============================================================================
// EaseType enum Tests
// ============================================================================

TEST_CASE("EaseType enum", "[cinematic][track]") {
    REQUIRE(static_cast<uint8_t>(EaseType::Linear) == 0);
    REQUIRE(static_cast<uint8_t>(EaseType::EaseIn) == 1);
    REQUIRE(static_cast<uint8_t>(EaseType::EaseOut) == 2);
    REQUIRE(static_cast<uint8_t>(EaseType::EaseInOut) == 3);
    REQUIRE(static_cast<uint8_t>(EaseType::EaseInQuad) == 4);
    REQUIRE(static_cast<uint8_t>(EaseType::EaseOutQuad) == 5);
    REQUIRE(static_cast<uint8_t>(EaseType::EaseInOutQuad) == 6);
    REQUIRE(static_cast<uint8_t>(EaseType::EaseInCubic) == 7);
    REQUIRE(static_cast<uint8_t>(EaseType::EaseOutCubic) == 8);
    REQUIRE(static_cast<uint8_t>(EaseType::EaseInOutCubic) == 9);
    REQUIRE(static_cast<uint8_t>(EaseType::EaseInElastic) == 10);
    REQUIRE(static_cast<uint8_t>(EaseType::EaseOutElastic) == 11);
    REQUIRE(static_cast<uint8_t>(EaseType::EaseOutBounce) == 12);
}

// ============================================================================
// TrackType enum Tests
// ============================================================================

TEST_CASE("TrackType enum", "[cinematic][track]") {
    REQUIRE(static_cast<uint8_t>(TrackType::Camera) == 0);
    REQUIRE(static_cast<uint8_t>(TrackType::Animation) == 1);
    REQUIRE(static_cast<uint8_t>(TrackType::Audio) == 2);
    REQUIRE(static_cast<uint8_t>(TrackType::Event) == 3);
    REQUIRE(static_cast<uint8_t>(TrackType::Property) == 4);
    REQUIRE(static_cast<uint8_t>(TrackType::Transform) == 5);
    REQUIRE(static_cast<uint8_t>(TrackType::Light) == 6);
    REQUIRE(static_cast<uint8_t>(TrackType::PostProcess) == 7);
}

// ============================================================================
// KeyframeBase Tests
// ============================================================================

TEST_CASE("KeyframeBase defaults", "[cinematic][keyframe]") {
    KeyframeBase kf;

    REQUIRE_THAT(kf.time, WithinAbs(0.0f, 0.001f));
    REQUIRE(kf.interpolation == InterpolationMode::Linear);
    REQUIRE(kf.easing == EaseType::Linear);
    REQUIRE_THAT(kf.tangent_in.x, WithinAbs(-0.1f, 0.001f));
    REQUIRE_THAT(kf.tangent_in.y, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(kf.tangent_out.x, WithinAbs(0.1f, 0.001f));
    REQUIRE_THAT(kf.tangent_out.y, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("KeyframeBase configuration", "[cinematic][keyframe]") {
    KeyframeBase kf;
    kf.time = 2.5f;
    kf.interpolation = InterpolationMode::Bezier;
    kf.easing = EaseType::EaseInOutCubic;
    kf.tangent_in = Vec2{-0.2f, 0.1f};
    kf.tangent_out = Vec2{0.2f, -0.1f};

    REQUIRE_THAT(kf.time, WithinAbs(2.5f, 0.001f));
    REQUIRE(kf.interpolation == InterpolationMode::Bezier);
    REQUIRE(kf.easing == EaseType::EaseInOutCubic);
    REQUIRE_THAT(kf.tangent_in.x, WithinAbs(-0.2f, 0.001f));
}

// ============================================================================
// Keyframe<T> Tests
// ============================================================================

TEST_CASE("Keyframe<float> default", "[cinematic][keyframe]") {
    Keyframe<float> kf;

    REQUIRE_THAT(kf.time, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(kf.value, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("Keyframe<float> constructor", "[cinematic][keyframe]") {
    Keyframe<float> kf(1.5f, 100.0f);

    REQUIRE_THAT(kf.time, WithinAbs(1.5f, 0.001f));
    REQUIRE_THAT(kf.value, WithinAbs(100.0f, 0.001f));
}

TEST_CASE("Keyframe<Vec3> constructor", "[cinematic][keyframe]") {
    Keyframe<Vec3> kf(2.0f, Vec3{1.0f, 2.0f, 3.0f});

    REQUIRE_THAT(kf.time, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(kf.value.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(kf.value.y, WithinAbs(2.0f, 0.001f));
    REQUIRE_THAT(kf.value.z, WithinAbs(3.0f, 0.001f));
}

TEST_CASE("Keyframe<Quat> constructor", "[cinematic][keyframe]") {
    Keyframe<Quat> kf(3.0f, Quat{1.0f, 0.0f, 0.0f, 0.0f});

    REQUIRE_THAT(kf.time, WithinAbs(3.0f, 0.001f));
    REQUIRE_THAT(kf.value.w, WithinAbs(1.0f, 0.001f));
}

// ============================================================================
// apply_easing Tests
// ============================================================================

TEST_CASE("apply_easing Linear", "[cinematic][easing]") {
    REQUIRE_THAT(apply_easing(0.0f, EaseType::Linear), WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(apply_easing(0.5f, EaseType::Linear), WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(apply_easing(1.0f, EaseType::Linear), WithinAbs(1.0f, 0.001f));
}

TEST_CASE("apply_easing EaseIn", "[cinematic][easing]") {
    REQUIRE_THAT(apply_easing(0.0f, EaseType::EaseIn), WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(apply_easing(1.0f, EaseType::EaseIn), WithinAbs(1.0f, 0.001f));

    // EaseIn should be slower at start
    float mid = apply_easing(0.5f, EaseType::EaseIn);
    REQUIRE(mid < 0.5f);
}

TEST_CASE("apply_easing EaseOut", "[cinematic][easing]") {
    REQUIRE_THAT(apply_easing(0.0f, EaseType::EaseOut), WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(apply_easing(1.0f, EaseType::EaseOut), WithinAbs(1.0f, 0.001f));

    // EaseOut should be faster at start
    float mid = apply_easing(0.5f, EaseType::EaseOut);
    REQUIRE(mid > 0.5f);
}

TEST_CASE("apply_easing EaseInOut", "[cinematic][easing]") {
    REQUIRE_THAT(apply_easing(0.0f, EaseType::EaseInOut), WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(apply_easing(0.5f, EaseType::EaseInOut), WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(apply_easing(1.0f, EaseType::EaseInOut), WithinAbs(1.0f, 0.001f));
}

TEST_CASE("apply_easing EaseOutBounce", "[cinematic][easing]") {
    REQUIRE_THAT(apply_easing(0.0f, EaseType::EaseOutBounce), WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(apply_easing(1.0f, EaseType::EaseOutBounce), WithinAbs(1.0f, 0.001f));

    // Bounce should produce values > 1 during overshoot
    // and eventually settle at 1
}

// ============================================================================
// interpolate_linear Tests
// ============================================================================

TEST_CASE("interpolate_linear float", "[cinematic][interpolation]") {
    float a = 0.0f;
    float b = 100.0f;

    REQUIRE_THAT(interpolate_linear(a, b, 0.0f), WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(interpolate_linear(a, b, 0.5f), WithinAbs(50.0f, 0.001f));
    REQUIRE_THAT(interpolate_linear(a, b, 1.0f), WithinAbs(100.0f, 0.001f));
}

TEST_CASE("interpolate_linear Vec3", "[cinematic][interpolation]") {
    Vec3 a{0.0f, 0.0f, 0.0f};
    Vec3 b{10.0f, 20.0f, 30.0f};

    Vec3 mid = interpolate_linear(a, b, 0.5f);
    REQUIRE_THAT(mid.x, WithinAbs(5.0f, 0.001f));
    REQUIRE_THAT(mid.y, WithinAbs(10.0f, 0.001f));
    REQUIRE_THAT(mid.z, WithinAbs(15.0f, 0.001f));
}
