#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/render/render_pipeline.hpp>

using namespace engine::render;
using Catch::Matchers::WithinAbs;

// --- apply_quality_preset_to_config() tests ---

TEST_CASE("Quality preset Low", "[render][quality]") {
    RenderPipelineConfig base;
    auto config = apply_quality_preset_to_config(base, RenderQuality::Low);

    REQUIRE(config.quality == RenderQuality::Low);
    REQUIRE_THAT(config.render_scale, WithinAbs(0.75f, 0.001f));

    // Shadow settings
    REQUIRE(config.shadow_config.cascade_resolution == 1024);
    REQUIRE(config.shadow_config.cascade_count == 2);

    // SSAO settings
    REQUIRE(config.ssao_config.sample_count == 8);
    REQUIRE(config.ssao_config.half_resolution == true);

    // Bloom and TAA disabled
    REQUIRE(config.bloom_config.enabled == false);
    REQUIRE(config.bloom_config.mip_count == 0);
    REQUIRE(config.taa_config.enabled == false);

    // Volumetric reduced
    REQUIRE(config.volumetric_config.froxel_depth == 32);

    // Pass flags: only essential passes
    REQUIRE(has_flag(config.enabled_passes, RenderPassFlags::Shadows));
    REQUIRE(has_flag(config.enabled_passes, RenderPassFlags::MainOpaque));
    REQUIRE(has_flag(config.enabled_passes, RenderPassFlags::Transparent));
    REQUIRE(has_flag(config.enabled_passes, RenderPassFlags::Final));
    REQUIRE_FALSE(has_flag(config.enabled_passes, RenderPassFlags::SSAO));
    REQUIRE_FALSE(has_flag(config.enabled_passes, RenderPassFlags::PostProcess));
    REQUIRE_FALSE(has_flag(config.enabled_passes, RenderPassFlags::TAA));
}

TEST_CASE("Quality preset Medium", "[render][quality]") {
    RenderPipelineConfig base;
    auto config = apply_quality_preset_to_config(base, RenderQuality::Medium);

    REQUIRE(config.quality == RenderQuality::Medium);
    REQUIRE_THAT(config.render_scale, WithinAbs(1.0f, 0.001f));

    // Shadow settings
    REQUIRE(config.shadow_config.cascade_resolution == 2048);
    REQUIRE(config.shadow_config.cascade_count == 3);

    // SSAO settings
    REQUIRE(config.ssao_config.sample_count == 16);
    REQUIRE(config.ssao_config.half_resolution == true);

    // Bloom enabled
    REQUIRE(config.bloom_config.enabled == true);
    REQUIRE(config.bloom_config.mip_count == 4);

    // TAA enabled
    REQUIRE(config.taa_config.enabled == true);

    // Volumetric moderate
    REQUIRE(config.volumetric_config.froxel_depth == 64);

    // Pass flags
    REQUIRE(has_flag(config.enabled_passes, RenderPassFlags::Shadows));
    REQUIRE(has_flag(config.enabled_passes, RenderPassFlags::SSAO));
    REQUIRE(has_flag(config.enabled_passes, RenderPassFlags::MainOpaque));
    REQUIRE(has_flag(config.enabled_passes, RenderPassFlags::Transparent));
    REQUIRE(has_flag(config.enabled_passes, RenderPassFlags::PostProcess));
    REQUIRE(has_flag(config.enabled_passes, RenderPassFlags::TAA));
    REQUIRE(has_flag(config.enabled_passes, RenderPassFlags::Final));
}

TEST_CASE("Quality preset High", "[render][quality]") {
    RenderPipelineConfig base;
    auto config = apply_quality_preset_to_config(base, RenderQuality::High);

    REQUIRE(config.quality == RenderQuality::High);
    REQUIRE_THAT(config.render_scale, WithinAbs(1.0f, 0.001f));

    // Shadow settings: 4 cascades at 2048
    REQUIRE(config.shadow_config.cascade_resolution == 2048);
    REQUIRE(config.shadow_config.cascade_count == 4);

    // SSAO: 32 samples, full res
    REQUIRE(config.ssao_config.sample_count == 32);
    REQUIRE(config.ssao_config.half_resolution == false);

    // Bloom: 5 mips
    REQUIRE(config.bloom_config.enabled == true);
    REQUIRE(config.bloom_config.mip_count == 5);

    // TAA enabled
    REQUIRE(config.taa_config.enabled == true);

    // Volumetric full
    REQUIRE(config.volumetric_config.froxel_depth == 128);

    // All passes enabled
    REQUIRE(config.enabled_passes == RenderPassFlags::All);
}

TEST_CASE("Quality preset Ultra", "[render][quality]") {
    RenderPipelineConfig base;
    auto config = apply_quality_preset_to_config(base, RenderQuality::Ultra);

    REQUIRE(config.quality == RenderQuality::Ultra);
    REQUIRE_THAT(config.render_scale, WithinAbs(1.0f, 0.001f));

    // Shadow settings: 4096 resolution, 49 PCF samples (7x7)
    REQUIRE(config.shadow_config.cascade_resolution == 4096);
    REQUIRE(config.shadow_config.cascade_count == 4);
    REQUIRE(config.shadow_config.pcf_samples == 49);

    // SSAO: 64 samples, full res
    REQUIRE(config.ssao_config.sample_count == 64);
    REQUIRE(config.ssao_config.half_resolution == false);

    // Bloom: 6 mips
    REQUIRE(config.bloom_config.enabled == true);
    REQUIRE(config.bloom_config.mip_count == 6);

    // TAA enabled
    REQUIRE(config.taa_config.enabled == true);

    // Volumetric: temporal reprojection enabled
    REQUIRE(config.volumetric_config.froxel_depth == 128);
    REQUIRE(config.volumetric_config.temporal_reprojection == true);

    // All passes enabled
    REQUIRE(config.enabled_passes == RenderPassFlags::All);
}

TEST_CASE("Quality preset Custom preserves existing settings", "[render][quality]") {
    RenderPipelineConfig base;
    base.render_scale = 1.5f;
    base.shadow_config.cascade_resolution = 512;
    base.shadow_config.cascade_count = 1;
    base.ssao_config.sample_count = 4;
    base.bloom_config.enabled = false;
    base.bloom_config.mip_count = 2;
    base.taa_config.enabled = false;
    base.volumetric_config.froxel_depth = 16;
    base.enabled_passes = RenderPassFlags::MainOpaque;

    auto config = apply_quality_preset_to_config(base, RenderQuality::Custom);

    REQUIRE(config.quality == RenderQuality::Custom);
    // All settings should be preserved from base
    REQUIRE_THAT(config.render_scale, WithinAbs(1.5f, 0.001f));
    REQUIRE(config.shadow_config.cascade_resolution == 512);
    REQUIRE(config.shadow_config.cascade_count == 1);
    REQUIRE(config.ssao_config.sample_count == 4);
    REQUIRE(config.bloom_config.enabled == false);
    REQUIRE(config.bloom_config.mip_count == 2);
    REQUIRE(config.taa_config.enabled == false);
    REQUIRE(config.volumetric_config.froxel_depth == 16);
    REQUIRE(config.enabled_passes == RenderPassFlags::MainOpaque);
}

// --- Test that presets don't modify the base config ---

TEST_CASE("apply_quality_preset_to_config does not modify input", "[render][quality]") {
    RenderPipelineConfig base;
    base.render_scale = 1.0f;
    base.shadow_config.cascade_resolution = 2048;

    RenderPipelineConfig original = base;
    auto result = apply_quality_preset_to_config(base, RenderQuality::Low);

    // base should not be modified
    REQUIRE_THAT(base.render_scale, WithinAbs(original.render_scale, 0.001f));
    REQUIRE(base.shadow_config.cascade_resolution == original.shadow_config.cascade_resolution);

    // result should differ from base
    REQUIRE_THAT(result.render_scale, WithinAbs(0.75f, 0.001f));
    REQUIRE(result.shadow_config.cascade_resolution == 1024);
}

// --- Test round-trip: Low overrides High and vice versa ---

TEST_CASE("Quality presets override each other correctly", "[render][quality]") {
    RenderPipelineConfig base;

    auto low = apply_quality_preset_to_config(base, RenderQuality::Low);
    REQUIRE(low.bloom_config.enabled == false);

    auto high_from_low = apply_quality_preset_to_config(low, RenderQuality::High);
    REQUIRE(high_from_low.bloom_config.enabled == true);
    REQUIRE(high_from_low.bloom_config.mip_count == 5);
}

// --- Test all presets set the quality field ---

TEST_CASE("All presets set the quality field correctly", "[render][quality]") {
    RenderPipelineConfig base;

    REQUIRE(apply_quality_preset_to_config(base, RenderQuality::Low).quality == RenderQuality::Low);
    REQUIRE(apply_quality_preset_to_config(base, RenderQuality::Medium).quality == RenderQuality::Medium);
    REQUIRE(apply_quality_preset_to_config(base, RenderQuality::High).quality == RenderQuality::High);
    REQUIRE(apply_quality_preset_to_config(base, RenderQuality::Ultra).quality == RenderQuality::Ultra);
    REQUIRE(apply_quality_preset_to_config(base, RenderQuality::Custom).quality == RenderQuality::Custom);
}
