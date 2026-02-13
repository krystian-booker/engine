#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/render/render_pipeline.hpp>

using namespace engine::render;
using Catch::Matchers::WithinAbs;

// --- RenderPassFlags bitwise operations ---

TEST_CASE("RenderPassFlags bitwise OR", "[render][pipeline_config]") {
    auto flags = RenderPassFlags::Shadows | RenderPassFlags::MainOpaque;
    REQUIRE(has_flag(flags, RenderPassFlags::Shadows));
    REQUIRE(has_flag(flags, RenderPassFlags::MainOpaque));
    REQUIRE_FALSE(has_flag(flags, RenderPassFlags::SSAO));
}

TEST_CASE("RenderPassFlags bitwise AND", "[render][pipeline_config]") {
    auto all = RenderPassFlags::All;
    auto masked = all & RenderPassFlags::Shadows;
    REQUIRE(has_flag(masked, RenderPassFlags::Shadows));
    REQUIRE_FALSE(has_flag(masked, RenderPassFlags::MainOpaque));
}

TEST_CASE("RenderPassFlags has_flag with None", "[render][pipeline_config]") {
    REQUIRE_FALSE(has_flag(RenderPassFlags::None, RenderPassFlags::Shadows));
    REQUIRE_FALSE(has_flag(RenderPassFlags::None, RenderPassFlags::MainOpaque));
}

TEST_CASE("RenderPassFlags AllOpaque combination", "[render][pipeline_config]") {
    auto flags = RenderPassFlags::AllOpaque;
    REQUIRE(has_flag(flags, RenderPassFlags::Shadows));
    REQUIRE(has_flag(flags, RenderPassFlags::DepthPrepass));
    REQUIRE(has_flag(flags, RenderPassFlags::MainOpaque));
    REQUIRE(has_flag(flags, RenderPassFlags::Skybox));
    REQUIRE_FALSE(has_flag(flags, RenderPassFlags::Transparent));
    REQUIRE_FALSE(has_flag(flags, RenderPassFlags::PostProcess));
}

TEST_CASE("RenderPassFlags AllEffects combination", "[render][pipeline_config]") {
    auto flags = RenderPassFlags::AllEffects;
    REQUIRE(has_flag(flags, RenderPassFlags::SSAO));
    REQUIRE(has_flag(flags, RenderPassFlags::Volumetric));
    REQUIRE(has_flag(flags, RenderPassFlags::Particles));
    REQUIRE(has_flag(flags, RenderPassFlags::SSR));
    REQUIRE(has_flag(flags, RenderPassFlags::PostProcess));
    REQUIRE(has_flag(flags, RenderPassFlags::TAA));
    REQUIRE_FALSE(has_flag(flags, RenderPassFlags::Shadows));
    REQUIRE_FALSE(has_flag(flags, RenderPassFlags::MainOpaque));
}

TEST_CASE("RenderPassFlags All includes everything", "[render][pipeline_config]") {
    auto flags = RenderPassFlags::All;
    REQUIRE(has_flag(flags, RenderPassFlags::Shadows));
    REQUIRE(has_flag(flags, RenderPassFlags::DepthPrepass));
    REQUIRE(has_flag(flags, RenderPassFlags::GBuffer));
    REQUIRE(has_flag(flags, RenderPassFlags::SSAO));
    REQUIRE(has_flag(flags, RenderPassFlags::MainOpaque));
    REQUIRE(has_flag(flags, RenderPassFlags::Volumetric));
    REQUIRE(has_flag(flags, RenderPassFlags::Transparent));
    REQUIRE(has_flag(flags, RenderPassFlags::Particles));
    REQUIRE(has_flag(flags, RenderPassFlags::SSR));
    REQUIRE(has_flag(flags, RenderPassFlags::PostProcess));
    REQUIRE(has_flag(flags, RenderPassFlags::TAA));
    REQUIRE(has_flag(flags, RenderPassFlags::Debug));
    REQUIRE(has_flag(flags, RenderPassFlags::UI));
    REQUIRE(has_flag(flags, RenderPassFlags::Final));
    REQUIRE(has_flag(flags, RenderPassFlags::Skybox));
}

TEST_CASE("RenderPassFlags individual bit values", "[render][pipeline_config]") {
    REQUIRE(static_cast<uint32_t>(RenderPassFlags::None) == 0);
    REQUIRE(static_cast<uint32_t>(RenderPassFlags::Shadows) == (1 << 0));
    REQUIRE(static_cast<uint32_t>(RenderPassFlags::DepthPrepass) == (1 << 1));
    REQUIRE(static_cast<uint32_t>(RenderPassFlags::GBuffer) == (1 << 2));
    REQUIRE(static_cast<uint32_t>(RenderPassFlags::SSAO) == (1 << 3));
    REQUIRE(static_cast<uint32_t>(RenderPassFlags::MainOpaque) == (1 << 4));
    REQUIRE(static_cast<uint32_t>(RenderPassFlags::Volumetric) == (1 << 5));
    REQUIRE(static_cast<uint32_t>(RenderPassFlags::Transparent) == (1 << 6));
    REQUIRE(static_cast<uint32_t>(RenderPassFlags::Particles) == (1 << 7));
    REQUIRE(static_cast<uint32_t>(RenderPassFlags::SSR) == (1 << 8));
    REQUIRE(static_cast<uint32_t>(RenderPassFlags::PostProcess) == (1 << 9));
    REQUIRE(static_cast<uint32_t>(RenderPassFlags::TAA) == (1 << 10));
    REQUIRE(static_cast<uint32_t>(RenderPassFlags::Debug) == (1 << 11));
    REQUIRE(static_cast<uint32_t>(RenderPassFlags::UI) == (1 << 12));
    REQUIRE(static_cast<uint32_t>(RenderPassFlags::Final) == (1 << 13));
    REQUIRE(static_cast<uint32_t>(RenderPassFlags::Skybox) == (1 << 14));
}

// --- RenderPipelineConfig defaults ---

TEST_CASE("RenderPipelineConfig default values", "[render][pipeline_config]") {
    RenderPipelineConfig config;

    REQUIRE(config.quality == RenderQuality::High);
    REQUIRE(config.enabled_passes == RenderPassFlags::All);
    REQUIRE(config.render_scale == 1.0f);
    REQUIRE(config.dynamic_resolution == false);
    REQUIRE_THAT(config.target_frametime_ms, WithinAbs(16.67f, 0.01f));
    REQUIRE(config.order_independent_transparency == false);
    REQUIRE(config.max_oit_layers == 4);
    REQUIRE(config.show_debug_overlay == false);
    REQUIRE(config.wireframe_mode == false);
}

TEST_CASE("RenderPipelineConfig shadow defaults", "[render][pipeline_config]") {
    RenderPipelineConfig config;

    REQUIRE(config.shadow_config.cascade_count == 4);
    REQUIRE(config.shadow_config.cascade_resolution == 2048);
    REQUIRE(config.shadow_config.pcf_enabled == true);
    REQUIRE(config.shadow_config.pcf_samples == 16);
}

TEST_CASE("RenderPipelineConfig SSAO defaults", "[render][pipeline_config]") {
    RenderPipelineConfig config;

    REQUIRE(config.ssao_config.sample_count == 32);
    REQUIRE(config.ssao_config.half_resolution == true);
    REQUIRE(config.ssao_config.blur_enabled == true);
}

TEST_CASE("RenderPipelineConfig bloom defaults", "[render][pipeline_config]") {
    RenderPipelineConfig config;

    REQUIRE(config.bloom_config.enabled == true);
    REQUIRE(config.bloom_config.mip_count == 5);
    REQUIRE_THAT(config.bloom_config.threshold, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("RenderPipelineConfig TAA defaults", "[render][pipeline_config]") {
    RenderPipelineConfig config;

    REQUIRE(config.taa_config.enabled == true);
    REQUIRE(config.taa_config.sharpen == true);
}

// --- RenderStats defaults ---

TEST_CASE("RenderStats all counters default to zero", "[render][pipeline_config]") {
    RenderStats stats;

    REQUIRE(stats.draw_calls == 0);
    REQUIRE(stats.triangles == 0);
    REQUIRE(stats.vertices == 0);
    REQUIRE(stats.objects_rendered == 0);
    REQUIRE(stats.objects_culled == 0);
    REQUIRE(stats.shadow_casters == 0);
    REQUIRE(stats.lights == 0);

    REQUIRE_THAT(stats.shadow_pass_ms, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(stats.depth_pass_ms, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(stats.ssao_pass_ms, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(stats.main_pass_ms, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(stats.volumetric_pass_ms, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(stats.transparent_pass_ms, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(stats.post_process_ms, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(stats.total_frame_ms, WithinAbs(0.0f, 0.001f));

    REQUIRE(stats.gpu_memory_used == 0);
    REQUIRE(stats.gpu_memory_total == 0);
}

// --- RenderQuality enum ---

TEST_CASE("RenderQuality enum values", "[render][pipeline_config]") {
    REQUIRE(static_cast<int>(RenderQuality::Low) == 0);
    REQUIRE(static_cast<int>(RenderQuality::Medium) == 1);
    REQUIRE(static_cast<int>(RenderQuality::High) == 2);
    REQUIRE(static_cast<int>(RenderQuality::Ultra) == 3);
    REQUIRE(static_cast<int>(RenderQuality::Custom) == 4);
}
