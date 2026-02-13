#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/render/render_target.hpp>
#include <engine/render/shadow_system.hpp>
#include <engine/render/ssao.hpp>
#include <engine/render/post_process.hpp>
#include <engine/render/volumetric.hpp>

using namespace engine::render;
using Catch::Matchers::WithinAbs;

// --- RenderTargetHandle ---

TEST_CASE("RenderTargetHandle default is invalid", "[render][rt_types]") {
    RenderTargetHandle h;
    REQUIRE_FALSE(h.valid());
    REQUIRE(h.id == UINT32_MAX);
}

TEST_CASE("RenderTargetHandle valid with assigned id", "[render][rt_types]") {
    RenderTargetHandle h;
    h.id = 42;
    REQUIRE(h.valid());
}

// --- RenderTargetDesc defaults ---

TEST_CASE("RenderTargetDesc defaults", "[render][rt_types]") {
    RenderTargetDesc desc;

    REQUIRE(desc.width == 0);
    REQUIRE(desc.height == 0);
    REQUIRE(desc.color_format == TextureFormat::RGBA16F);
    REQUIRE(desc.color_attachment_count == 1);
    REQUIRE(desc.depth_format == TextureFormat::Depth32F);
    REQUIRE(desc.has_depth == true);
    REQUIRE(desc.msaa_samples == 1);
    REQUIRE(desc.generate_mipmaps == false);
    REQUIRE(desc.samplable == true);
    REQUIRE(desc.debug_name == nullptr);
}

// --- ViewConfig defaults ---

TEST_CASE("ViewConfig defaults", "[render][rt_types]") {
    ViewConfig config;

    REQUIRE(config.clear_color == 0x000000ff);
    REQUIRE_THAT(config.clear_depth, WithinAbs(1.0f, 0.001f));
    REQUIRE(config.clear_stencil == 0);
    REQUIRE(config.clear_color_enabled == true);
    REQUIRE(config.clear_depth_enabled == true);
    REQUIRE(config.clear_stencil_enabled == false);
    REQUIRE(config.viewport_x == 0);
    REQUIRE(config.viewport_y == 0);
    REQUIRE(config.viewport_width == 0);
    REQUIRE(config.viewport_height == 0);
    REQUIRE_FALSE(config.render_target.valid());
}

// --- RenderView enum values ---

TEST_CASE("RenderView shadow cascade values", "[render][rt_types]") {
    REQUIRE(static_cast<uint16_t>(RenderView::ShadowCascade0) == 0);
    REQUIRE(static_cast<uint16_t>(RenderView::ShadowCascade1) == 1);
    REQUIRE(static_cast<uint16_t>(RenderView::ShadowCascade2) == 2);
    REQUIRE(static_cast<uint16_t>(RenderView::ShadowCascade3) == 3);
}

TEST_CASE("RenderView main pass values", "[render][rt_types]") {
    REQUIRE(static_cast<uint16_t>(RenderView::MainOpaque) == 40);
    REQUIRE(static_cast<uint16_t>(RenderView::MainTransparent) == 41);
}

TEST_CASE("RenderView final output value", "[render][rt_types]") {
    REQUIRE(static_cast<uint16_t>(RenderView::Final) == 64);
}

TEST_CASE("RenderView count", "[render][rt_types]") {
    REQUIRE(static_cast<uint16_t>(RenderView::Count) == 80);
}

TEST_CASE("RenderView ToneMap alias", "[render][rt_types]") {
    REQUIRE(RenderView::ToneMap == RenderView::Tonemapping);
    REQUIRE(static_cast<uint16_t>(RenderView::ToneMap) == 61);
}

TEST_CASE("RenderView screen-space effect views", "[render][rt_types]") {
    REQUIRE(static_cast<uint16_t>(RenderView::SSAO) == 35);
    REQUIRE(static_cast<uint16_t>(RenderView::SSAOBlur) == 36);
    REQUIRE(static_cast<uint16_t>(RenderView::SSR) == 37);
}

TEST_CASE("RenderView special views", "[render][rt_types]") {
    REQUIRE(static_cast<uint16_t>(RenderView::DepthPrepass) == 32);
    REQUIRE(static_cast<uint16_t>(RenderView::GBuffer) == 33);
    REQUIRE(static_cast<uint16_t>(RenderView::MotionVectors) == 34);
    REQUIRE(static_cast<uint16_t>(RenderView::Debug) == 62);
    REQUIRE(static_cast<uint16_t>(RenderView::UI) == 63);
    REQUIRE(static_cast<uint16_t>(RenderView::Skybox) == 39);
}

// --- ShadowConfig defaults ---

TEST_CASE("ShadowConfig defaults", "[render][rt_types]") {
    ShadowConfig config;

    REQUIRE(config.cascade_count == 4);
    REQUIRE(config.cascade_resolution == 2048);
    REQUIRE(config.point_light_resolution == 512);
    REQUIRE(config.spot_light_resolution == 1024);
    REQUIRE(config.max_shadow_casting_lights == 4);
    REQUIRE_THAT(config.shadow_bias, WithinAbs(0.001f, 0.0001f));
    REQUIRE_THAT(config.normal_bias, WithinAbs(0.01f, 0.001f));
    REQUIRE_THAT(config.cascade_blend_distance, WithinAbs(0.1f, 0.01f));
    REQUIRE(config.pcf_enabled == true);
    REQUIRE(config.pcf_samples == 16);
}

// --- SSAOConfig defaults ---

TEST_CASE("SSAOConfig defaults", "[render][rt_types]") {
    SSAOConfig config;

    REQUIRE(config.sample_count == 32);
    REQUIRE_THAT(config.radius, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(config.bias, WithinAbs(0.025f, 0.001f));
    REQUIRE_THAT(config.intensity, WithinAbs(1.5f, 0.001f));
    REQUIRE_THAT(config.power, WithinAbs(2.0f, 0.001f));
    REQUIRE(config.half_resolution == true);
    REQUIRE(config.blur_enabled == true);
    REQUIRE(config.blur_passes == 2);
}

// --- BloomConfig defaults ---

TEST_CASE("BloomConfig defaults", "[render][rt_types]") {
    BloomConfig config;

    REQUIRE(config.enabled == true);
    REQUIRE_THAT(config.threshold, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(config.intensity, WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(config.scatter, WithinAbs(0.7f, 0.001f));
    REQUIRE(config.mip_count == 5);
}

// --- ToneMappingConfig defaults ---

TEST_CASE("ToneMappingConfig defaults", "[render][rt_types]") {
    ToneMappingConfig config;

    REQUIRE(config.op == ToneMappingOperator::ACES);
    REQUIRE_THAT(config.exposure, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(config.gamma, WithinAbs(2.2f, 0.001f));
    REQUIRE_THAT(config.white_point, WithinAbs(4.0f, 0.001f));
    REQUIRE(config.auto_exposure == false);
    REQUIRE_THAT(config.adaptation_speed, WithinAbs(1.0f, 0.001f));
}

// --- TAAConfig defaults ---

TEST_CASE("TAAConfig defaults", "[render][rt_types]") {
    TAAConfig config;

    REQUIRE(config.enabled == true);
    REQUIRE_THAT(config.jitter_scale, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(config.feedback_min, WithinAbs(0.88f, 0.001f));
    REQUIRE_THAT(config.feedback_max, WithinAbs(0.97f, 0.001f));
    REQUIRE(config.sharpen == true);
    REQUIRE_THAT(config.sharpen_amount, WithinAbs(0.25f, 0.001f));
}

// --- VolumetricConfig defaults ---

TEST_CASE("VolumetricConfig defaults", "[render][rt_types]") {
    VolumetricConfig config;

    REQUIRE(config.froxel_width == 160);
    REQUIRE(config.froxel_height == 90);
    REQUIRE(config.froxel_depth == 128);
    REQUIRE_THAT(config.fog_density, WithinAbs(0.01f, 0.001f));
    REQUIRE_THAT(config.anisotropy, WithinAbs(0.5f, 0.001f));
    REQUIRE(config.temporal_reprojection == true);
    REQUIRE_THAT(config.temporal_blend, WithinAbs(0.9f, 0.001f));
    REQUIRE(config.shadows_enabled == true);
    REQUIRE(config.animated_noise == true);
}
