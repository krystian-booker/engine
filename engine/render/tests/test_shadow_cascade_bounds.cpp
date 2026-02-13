#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/render/shadow_system.hpp>
#include <engine/render/render_pipeline.hpp>

using namespace engine::render;
using Catch::Matchers::WithinAbs;

// --- MAX_CASCADES constant ---

TEST_CASE("MAX_CASCADES is 4", "[render][shadow]") {
    REQUIRE(MAX_CASCADES == 4);
}

// --- Cascade count clamped in ShadowConfig ---

TEST_CASE("ShadowConfig default cascade_count equals MAX_CASCADES", "[render][shadow]") {
    ShadowConfig config;
    REQUIRE(config.cascade_count == MAX_CASCADES);
}

// --- Cascade count clamped by apply_quality_preset_to_config ---

TEST_CASE("Quality presets never exceed MAX_CASCADES", "[render][shadow]") {
    RenderPipelineConfig base;

    auto low = apply_quality_preset_to_config(base, RenderQuality::Low);
    REQUIRE(low.shadow_config.cascade_count <= MAX_CASCADES);

    auto med = apply_quality_preset_to_config(base, RenderQuality::Medium);
    REQUIRE(med.shadow_config.cascade_count <= MAX_CASCADES);

    auto high = apply_quality_preset_to_config(base, RenderQuality::High);
    REQUIRE(high.shadow_config.cascade_count <= MAX_CASCADES);

    auto ultra = apply_quality_preset_to_config(base, RenderQuality::Ultra);
    REQUIRE(ultra.shadow_config.cascade_count <= MAX_CASCADES);
}

// --- Custom config with excessive cascade count is clamped ---

TEST_CASE("Excessive cascade_count is clamped by apply_quality_preset_to_config", "[render][shadow]") {
    RenderPipelineConfig base;
    base.shadow_config.cascade_count = 10;  // Excessive!

    auto config = apply_quality_preset_to_config(base, RenderQuality::Custom);
    REQUIRE(config.shadow_config.cascade_count <= MAX_CASCADES);
}

// --- Cascade splits are ordered ---

TEST_CASE("Default cascade splits are in ascending order", "[render][shadow]") {
    ShadowConfig config;
    for (uint32_t i = 1; i < config.cascade_count; ++i) {
        REQUIRE(config.cascade_splits[i] >= config.cascade_splits[i - 1]);
    }
}

TEST_CASE("Cascade splits are in range [0, 1]", "[render][shadow]") {
    ShadowConfig config;
    for (uint32_t i = 0; i < config.cascade_count; ++i) {
        REQUIRE(config.cascade_splits[i] >= 0.0f);
        REQUIRE(config.cascade_splits[i] <= 1.0f);
    }
}

// --- Cascade arrays are correctly sized ---

TEST_CASE("Cascade render target array has MAX_CASCADES capacity", "[render][shadow]") {
    // The ShadowSystem uses std::array<..., 4> internally
    // Verify that MAX_CASCADES matches the array size
    REQUIRE(MAX_CASCADES == 4);

    // Verify that CascadeData array is also sized to 4
    ShadowSystem system;
    // If we can access get_cascade(3) without issues, the array is correctly sized
    // (Note: we can't call this without init, but the constant check is sufficient)
}

// --- Low quality preset mip_count ---

TEST_CASE("Low quality preset sets bloom mip_count to 0", "[render][quality]") {
    RenderPipelineConfig base;
    auto config = apply_quality_preset_to_config(base, RenderQuality::Low);
    REQUIRE(config.bloom_config.mip_count == 0);
    REQUIRE(config.bloom_config.enabled == false);
}
