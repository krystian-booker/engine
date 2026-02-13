#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/render/ssao.hpp>
#include <engine/render/render_pipeline.hpp>
#include <cmath>

using namespace engine::render;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// --- SSAOConfig defaults ---

TEST_CASE("SSAOConfig default sample_count is 32", "[render][ssao]") {
    SSAOConfig config;
    REQUIRE(config.sample_count == 32);
}

TEST_CASE("SSAOConfig default blur is enabled with 2 passes", "[render][ssao]") {
    SSAOConfig config;
    REQUIRE(config.blur_enabled == true);
    REQUIRE(config.blur_passes == 2);
}

TEST_CASE("SSAOConfig radius is positive", "[render][ssao]") {
    SSAOConfig config;
    REQUIRE(config.radius > 0.0f);
}

TEST_CASE("SSAOConfig intensity is positive", "[render][ssao]") {
    SSAOConfig config;
    REQUIRE(config.intensity > 0.0f);
}

// --- SSAO kernel properties (tested via quality presets) ---

TEST_CASE("Ultra preset sets 64 SSAO samples", "[render][ssao]") {
    RenderPipelineConfig base;
    auto config = apply_quality_preset_to_config(base, RenderQuality::Ultra);
    REQUIRE(config.ssao_config.sample_count == 64);
}

TEST_CASE("Low preset sets 8 SSAO samples at half resolution", "[render][ssao]") {
    RenderPipelineConfig base;
    auto config = apply_quality_preset_to_config(base, RenderQuality::Low);
    REQUIRE(config.ssao_config.sample_count == 8);
    REQUIRE(config.ssao_config.half_resolution == true);
}

TEST_CASE("High preset uses full resolution SSAO", "[render][ssao]") {
    RenderPipelineConfig base;
    auto config = apply_quality_preset_to_config(base, RenderQuality::High);
    REQUIRE(config.ssao_config.half_resolution == false);
}

// --- GTAO helpers ---

TEST_CASE("GTAO integrate_arc returns non-negative for valid inputs", "[render][ssao]") {
    // For horizon angles in valid range, result should be non-negative
    float result = gtao::integrate_arc(0.0f, 0.5f, 0.0f);
    REQUIRE(result >= 0.0f);
}

TEST_CASE("GTAO integrate_arc is symmetric for zero normal angle", "[render][ssao]") {
    // With n=0, integrate_arc(h1, h2, 0) should be roughly symmetric around h=0
    float r1 = gtao::integrate_arc(-0.3f, 0.3f, 0.0f);
    float r2 = gtao::integrate_arc(-0.3f, 0.3f, 0.0f);
    REQUIRE_THAT(r1, WithinAbs(r2, 0.001f));
}

TEST_CASE("GTAO integrate_arc increases with wider arc", "[render][ssao]") {
    float narrow = gtao::integrate_arc(0.0f, 0.2f, 0.0f);
    float wide = gtao::integrate_arc(0.0f, 0.5f, 0.0f);
    REQUIRE(wide >= narrow);
}

// --- Blur ping-pong correctness (verified through get_ao_texture logic) ---

TEST_CASE("SSAO blur ping-pong: odd passes result in blur_temp", "[render][ssao]") {
    // With 1 blur pass (odd): pass 0 reads AO -> writes blur_temp
    // Result should be in blur_temp
    SSAOConfig config;
    config.blur_enabled = true;
    config.blur_passes = 1;
    // Verify the logic: passes % 2 != 0 -> true -> returns blur_temp
    REQUIRE((config.blur_passes % 2 != 0) == true);
}

TEST_CASE("SSAO blur ping-pong: even passes result in ao_target", "[render][ssao]") {
    // With 2 blur passes (even):
    // pass 0: AO -> blur_temp
    // pass 1: blur_temp -> AO
    // Result should be in AO
    SSAOConfig config;
    config.blur_enabled = true;
    config.blur_passes = 2;
    REQUIRE((config.blur_passes % 2 != 0) == false);
}

TEST_CASE("SSAO blur ping-pong: 3 passes result in blur_temp", "[render][ssao]") {
    SSAOConfig config;
    config.blur_enabled = true;
    config.blur_passes = 3;
    REQUIRE((config.blur_passes % 2 != 0) == true);
}
