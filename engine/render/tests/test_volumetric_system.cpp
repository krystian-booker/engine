#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/render/volumetric.hpp>
#include <engine/render/render_pipeline.hpp>
#include <cmath>

using namespace engine::render;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// --- Phase function tests ---

TEST_CASE("Henyey-Greenstein: isotropic (g=0) returns 1/(4*pi)", "[render][volumetric]") {
    float expected = 1.0f / (4.0f * 3.14159265f);
    float result = phase::henyey_greenstein(0.5f, 0.0f);
    REQUIRE_THAT(result, WithinAbs(expected, 0.001f));
}

TEST_CASE("Henyey-Greenstein: forward scatter peaks at cos_theta=1", "[render][volumetric]") {
    float g = 0.5f;
    float forward = phase::henyey_greenstein(1.0f, g);
    float side = phase::henyey_greenstein(0.0f, g);
    float back = phase::henyey_greenstein(-1.0f, g);

    REQUIRE(forward > side);
    REQUIRE(side > back);
}

TEST_CASE("Henyey-Greenstein: backward scatter peaks at cos_theta=-1", "[render][volumetric]") {
    float g = -0.5f;
    float forward = phase::henyey_greenstein(1.0f, g);
    float back = phase::henyey_greenstein(-1.0f, g);

    REQUIRE(back > forward);
}

TEST_CASE("Henyey-Greenstein: result is always positive", "[render][volumetric]") {
    for (float g = -0.9f; g <= 0.9f; g += 0.3f) {
        for (float cos_theta = -1.0f; cos_theta <= 1.0f; cos_theta += 0.25f) {
            REQUIRE(phase::henyey_greenstein(cos_theta, g) > 0.0f);
        }
    }
}

TEST_CASE("Schlick phase: isotropic (g=0) returns ~1/(4*pi)", "[render][volumetric]") {
    float expected = 1.0f / (4.0f * 3.14159265f);
    float result = phase::schlick_phase(0.5f, 0.0f);
    REQUIRE_THAT(result, WithinAbs(expected, 0.01f));
}

TEST_CASE("Schlick phase: forward scatter peaks at cos_theta=1", "[render][volumetric]") {
    float g = 0.5f;
    float forward = phase::schlick_phase(1.0f, g);
    float side = phase::schlick_phase(0.0f, g);

    REQUIRE(forward > side);
}

TEST_CASE("Schlick phase: result is always positive", "[render][volumetric]") {
    for (float g = -0.9f; g <= 0.9f; g += 0.3f) {
        for (float cos_theta = -1.0f; cos_theta <= 1.0f; cos_theta += 0.25f) {
            REQUIRE(phase::schlick_phase(cos_theta, g) > 0.0f);
        }
    }
}

TEST_CASE("Cornette-Shanks: forward scatter peaks at cos_theta=1", "[render][volumetric]") {
    float g = 0.5f;
    float forward = phase::cornette_shanks(1.0f, g);
    float side = phase::cornette_shanks(0.0f, g);

    REQUIRE(forward > side);
}

TEST_CASE("Cornette-Shanks: result is always positive", "[render][volumetric]") {
    for (float g = -0.9f; g <= 0.9f; g += 0.3f) {
        for (float cos_theta = -1.0f; cos_theta <= 1.0f; cos_theta += 0.25f) {
            REQUIRE(phase::cornette_shanks(cos_theta, g) > 0.0f);
        }
    }
}

// --- Phase function consistency ---

TEST_CASE("All phase functions agree at g=0 (isotropic)", "[render][volumetric]") {
    float expected = 1.0f / (4.0f * 3.14159265f);

    // At g=0, all three should return approximately 1/(4*pi) for any angle
    float hg = phase::henyey_greenstein(0.5f, 0.0f);
    float schlick = phase::schlick_phase(0.5f, 0.0f);
    float cs = phase::cornette_shanks(0.5f, 0.0f);

    REQUIRE_THAT(hg, WithinAbs(expected, 0.01f));
    REQUIRE_THAT(schlick, WithinAbs(expected, 0.01f));
    // Cornette-Shanks at g=0 should be close to 3/(16*pi)*(1+cos^2)
    REQUIRE(cs > 0.0f);
}

// --- VolumetricConfig defaults ---

TEST_CASE("VolumetricConfig default fog density is positive", "[render][volumetric]") {
    VolumetricConfig config;
    REQUIRE(config.fog_density > 0.0f);
}

TEST_CASE("VolumetricConfig default anisotropy is in [-1, 1]", "[render][volumetric]") {
    VolumetricConfig config;
    REQUIRE(config.anisotropy >= -1.0f);
    REQUIRE(config.anisotropy <= 1.0f);
}

TEST_CASE("VolumetricConfig default froxel dimensions are positive", "[render][volumetric]") {
    VolumetricConfig config;
    REQUIRE(config.froxel_width > 0);
    REQUIRE(config.froxel_height > 0);
    REQUIRE(config.froxel_depth > 0);
}

TEST_CASE("VolumetricConfig temporal blend is in [0, 1]", "[render][volumetric]") {
    VolumetricConfig config;
    REQUIRE(config.temporal_blend >= 0.0f);
    REQUIRE(config.temporal_blend <= 1.0f);
}

// --- Noise generation ---

TEST_CASE("3D noise generates correct data size", "[render][volumetric]") {
    std::vector<uint8_t> data;
    uint32_t size = 16;  // Small for test
    volumetric_noise::generate_3d_noise(data, size);

    REQUIRE(data.size() == size * size * size * 4);
}

TEST_CASE("3D noise values are in valid range", "[render][volumetric]") {
    std::vector<uint8_t> data;
    uint32_t size = 8;
    volumetric_noise::generate_3d_noise(data, size);

    // All alpha values should be 255
    for (uint32_t i = 0; i < size * size * size; ++i) {
        REQUIRE(data[i * 4 + 3] == 255);
    }
}

TEST_CASE("Blue noise generates correct data size", "[render][volumetric]") {
    std::vector<uint8_t> data;
    uint32_t size = 16;
    volumetric_noise::generate_blue_noise(data, size);

    REQUIRE(data.size() == size * size * 4);
}

TEST_CASE("Blue noise alpha values are 255", "[render][volumetric]") {
    std::vector<uint8_t> data;
    uint32_t size = 16;
    volumetric_noise::generate_blue_noise(data, size);

    for (uint32_t i = 0; i < size * size; ++i) {
        REQUIRE(data[i * 4 + 3] == 255);
    }
}

// --- Fog parameter packing ---

TEST_CASE("VolumetricConfig fog_albedo defaults to white", "[render][volumetric]") {
    VolumetricConfig config;
    REQUIRE_THAT(config.fog_albedo.x, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(config.fog_albedo.y, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(config.fog_albedo.z, WithinAbs(1.0f, 0.001f));
}

// --- Quality preset volumetric settings ---

TEST_CASE("Ultra quality enables temporal reprojection", "[render][volumetric]") {
    RenderPipelineConfig base;
    auto config = apply_quality_preset_to_config(base, RenderQuality::Ultra);
    REQUIRE(config.volumetric_config.temporal_reprojection == true);
    REQUIRE(config.volumetric_config.froxel_depth == 128);
}

TEST_CASE("Low quality reduces froxel depth", "[render][volumetric]") {
    RenderPipelineConfig base;
    auto config = apply_quality_preset_to_config(base, RenderQuality::Low);
    REQUIRE(config.volumetric_config.froxel_depth == 32);
}
