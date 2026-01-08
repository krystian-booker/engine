#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/environment/sky_controller.hpp>

using namespace engine::environment;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// ============================================================================
// SkyGradient Tests
// ============================================================================

TEST_CASE("SkyGradient defaults", "[environment][sky]") {
    SkyGradient gradient;

    REQUIRE_THAT(gradient.zenith_color.x, WithinAbs(0.2f, 0.001f));
    REQUIRE_THAT(gradient.zenith_color.y, WithinAbs(0.4f, 0.001f));
    REQUIRE_THAT(gradient.zenith_color.z, WithinAbs(0.8f, 0.001f));

    REQUIRE_THAT(gradient.horizon_color.x, WithinAbs(0.7f, 0.001f));
    REQUIRE_THAT(gradient.horizon_color.y, WithinAbs(0.8f, 0.001f));
    REQUIRE_THAT(gradient.horizon_color.z, WithinAbs(0.95f, 0.001f));

    REQUIRE_THAT(gradient.ground_color.x, WithinAbs(0.3f, 0.001f));
    REQUIRE_THAT(gradient.ground_color.y, WithinAbs(0.25f, 0.001f));
    REQUIRE_THAT(gradient.ground_color.z, WithinAbs(0.2f, 0.001f));
}

TEST_CASE("SkyGradient sunset colors", "[environment][sky]") {
    SkyGradient gradient;
    gradient.zenith_color = Vec3{0.3f, 0.2f, 0.5f};      // Purple/blue
    gradient.horizon_color = Vec3{1.0f, 0.5f, 0.2f};     // Orange
    gradient.ground_color = Vec3{0.2f, 0.15f, 0.1f};     // Dark brown

    REQUIRE_THAT(gradient.zenith_color.x, WithinAbs(0.3f, 0.001f));
    REQUIRE_THAT(gradient.horizon_color.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(gradient.horizon_color.y, WithinAbs(0.5f, 0.001f));
}

TEST_CASE("SkyGradient night colors", "[environment][sky]") {
    SkyGradient gradient;
    gradient.zenith_color = Vec3{0.01f, 0.02f, 0.05f};   // Near black
    gradient.horizon_color = Vec3{0.05f, 0.08f, 0.15f};  // Dark blue
    gradient.ground_color = Vec3{0.02f, 0.02f, 0.02f};   // Very dark

    REQUIRE_THAT(gradient.zenith_color.x, WithinAbs(0.01f, 0.001f));
    REQUIRE_THAT(gradient.zenith_color.z, WithinAbs(0.05f, 0.001f));
}

// ============================================================================
// SkyPreset Tests
// ============================================================================

TEST_CASE("SkyPreset defaults", "[environment][sky]") {
    SkyPreset preset;

    REQUIRE(preset.name.empty());

    // Sun parameters
    REQUIRE_THAT(preset.sun_size, WithinAbs(0.04f, 0.001f));
    REQUIRE_THAT(preset.sun_color.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(preset.sun_color.y, WithinAbs(0.95f, 0.001f));
    REQUIRE_THAT(preset.sun_color.z, WithinAbs(0.85f, 0.001f));
    REQUIRE_THAT(preset.sun_intensity, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(preset.sun_halo_color.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(preset.sun_halo_size, WithinAbs(0.15f, 0.001f));

    // Moon parameters
    REQUIRE_THAT(preset.moon_size, WithinAbs(0.025f, 0.001f));
    REQUIRE_THAT(preset.moon_color.x, WithinAbs(0.9f, 0.001f));
    REQUIRE_THAT(preset.moon_intensity, WithinAbs(0.3f, 0.001f));

    // Stars
    REQUIRE_THAT(preset.star_intensity, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(preset.star_twinkle_speed, WithinAbs(1.0f, 0.001f));

    // Clouds
    REQUIRE_THAT(preset.cloud_coverage, WithinAbs(0.3f, 0.001f));
    REQUIRE_THAT(preset.cloud_color.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(preset.cloud_brightness, WithinAbs(1.0f, 0.001f));

    // Atmosphere
    REQUIRE_THAT(preset.atmosphere_density, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(preset.mie_scattering, WithinAbs(0.02f, 0.001f));
    REQUIRE_THAT(preset.horizon_fog, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("SkyPreset dawn configuration", "[environment][sky]") {
    SkyPreset preset;
    preset.name = "dawn";
    preset.colors.zenith_color = Vec3{0.3f, 0.3f, 0.5f};
    preset.colors.horizon_color = Vec3{0.9f, 0.6f, 0.4f};
    preset.sun_intensity = 0.3f;
    preset.sun_color = Vec3{1.0f, 0.7f, 0.4f};  // Orange sunrise
    preset.star_intensity = 0.2f;  // Fading stars
    preset.cloud_coverage = 0.2f;
    preset.horizon_fog = 0.3f;

    REQUIRE(preset.name == "dawn");
    REQUIRE_THAT(preset.sun_intensity, WithinAbs(0.3f, 0.001f));
    REQUIRE_THAT(preset.star_intensity, WithinAbs(0.2f, 0.001f));
    REQUIRE_THAT(preset.horizon_fog, WithinAbs(0.3f, 0.001f));
}

TEST_CASE("SkyPreset noon configuration", "[environment][sky]") {
    SkyPreset preset;
    preset.name = "noon";
    preset.colors.zenith_color = Vec3{0.1f, 0.3f, 0.8f};   // Deep blue
    preset.colors.horizon_color = Vec3{0.5f, 0.7f, 0.95f}; // Light blue
    preset.sun_intensity = 1.2f;
    preset.sun_color = Vec3{1.0f, 0.98f, 0.95f};  // Near white
    preset.star_intensity = 0.0f;  // No stars
    preset.cloud_coverage = 0.3f;
    preset.atmosphere_density = 1.0f;

    REQUIRE(preset.name == "noon");
    REQUIRE_THAT(preset.sun_intensity, WithinAbs(1.2f, 0.001f));
    REQUIRE_THAT(preset.star_intensity, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("SkyPreset night configuration", "[environment][sky]") {
    SkyPreset preset;
    preset.name = "night";
    preset.colors.zenith_color = Vec3{0.01f, 0.02f, 0.06f};
    preset.colors.horizon_color = Vec3{0.05f, 0.08f, 0.15f};
    preset.sun_intensity = 0.0f;
    preset.moon_intensity = 0.3f;
    preset.star_intensity = 1.0f;  // Full stars
    preset.star_twinkle_speed = 1.5f;
    preset.cloud_coverage = 0.2f;

    REQUIRE(preset.name == "night");
    REQUIRE_THAT(preset.sun_intensity, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(preset.moon_intensity, WithinAbs(0.3f, 0.001f));
    REQUIRE_THAT(preset.star_intensity, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("SkyPreset overcast configuration", "[environment][sky]") {
    SkyPreset preset;
    preset.name = "overcast";
    preset.colors.zenith_color = Vec3{0.4f, 0.45f, 0.5f};
    preset.colors.horizon_color = Vec3{0.5f, 0.55f, 0.6f};
    preset.sun_intensity = 0.3f;  // Dimmed
    preset.cloud_coverage = 1.0f; // Full cloud cover
    preset.cloud_color = Vec3{0.7f, 0.7f, 0.7f};
    preset.cloud_brightness = 0.8f;
    preset.horizon_fog = 0.2f;

    REQUIRE(preset.name == "overcast");
    REQUIRE_THAT(preset.cloud_coverage, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(preset.sun_intensity, WithinAbs(0.3f, 0.001f));
    REQUIRE_THAT(preset.horizon_fog, WithinAbs(0.2f, 0.001f));
}

TEST_CASE("SkyPreset stormy configuration", "[environment][sky]") {
    SkyPreset preset;
    preset.name = "stormy";
    preset.colors.zenith_color = Vec3{0.15f, 0.18f, 0.22f};
    preset.colors.horizon_color = Vec3{0.25f, 0.28f, 0.32f};
    preset.sun_intensity = 0.1f;  // Very dim
    preset.cloud_coverage = 1.0f;
    preset.cloud_color = Vec3{0.3f, 0.32f, 0.35f};  // Dark grey
    preset.cloud_brightness = 0.5f;
    preset.horizon_fog = 0.5f;

    REQUIRE(preset.name == "stormy");
    REQUIRE_THAT(preset.sun_intensity, WithinAbs(0.1f, 0.001f));
    REQUIRE_THAT(preset.cloud_brightness, WithinAbs(0.5f, 0.001f));
}
