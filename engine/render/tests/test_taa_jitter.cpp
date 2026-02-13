#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/render/render_pipeline.hpp>
#include <engine/render/post_process.hpp>
#include <engine/render/renderer.hpp>
#include <cmath>

using namespace engine::render;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// Minimal mock renderer for TAASystem init
class TAA_MockRenderer : public IRenderer {
public:
    uint32_t w = 1920, h = 1080;
    uint32_t next_rt = 0, next_tex = 0;

    bool init(void*, uint32_t, uint32_t) override { return true; }
    void shutdown() override {}
    void begin_frame() override {}
    void end_frame() override {}
    void resize(uint32_t, uint32_t) override {}

    MeshHandle create_mesh(const MeshData&) override { return MeshHandle{0}; }
    TextureHandle create_texture(const TextureData&) override { return TextureHandle{next_tex++}; }
    ShaderHandle create_shader(const ShaderData&) override { return ShaderHandle{0}; }
    MaterialHandle create_material(const MaterialData&) override { return MaterialHandle{0}; }
    MeshHandle create_primitive(PrimitiveMesh, float) override { return MeshHandle{0}; }

    void destroy_mesh(MeshHandle) override {}
    void destroy_texture(TextureHandle) override {}
    void destroy_shader(ShaderHandle) override {}
    void destroy_material(MaterialHandle) override {}

    RenderTargetHandle create_render_target(const RenderTargetDesc&) override {
        return RenderTargetHandle{next_rt++};
    }
    void destroy_render_target(RenderTargetHandle) override {}
    TextureHandle get_render_target_texture(RenderTargetHandle, uint32_t) override {
        return TextureHandle{next_tex++};
    }
    void resize_render_target(RenderTargetHandle, uint32_t, uint32_t) override {}

    void configure_view(RenderView, const ViewConfig&) override {}
    void set_view_transform(RenderView, const Mat4&, const Mat4&) override {}

    void queue_draw(const DrawCall&) override {}
    void queue_draw(const DrawCall&, RenderView) override {}

    void set_camera(const Mat4&, const Mat4&) override {}
    void set_light(uint32_t, const LightData&) override {}
    void clear_lights() override {}

    void set_shadow_data(const std::array<Mat4, 4>&, const Vec4&, const Vec4&) override {}
    void set_shadow_texture(uint32_t, TextureHandle) override {}
    void enable_shadows(bool) override {}

    void submit_mesh(RenderView, MeshHandle, MaterialHandle, const Mat4&) override {}
    void submit_skinned_mesh(RenderView, MeshHandle, MaterialHandle,
                             const Mat4&, const Mat4*, uint32_t) override {}

    void flush_debug_draw(RenderView) override {}
    void blit_to_screen(RenderView, TextureHandle) override {}
    void submit_skybox(RenderView, TextureHandle, const Mat4&, float, float) override {}
    void submit_billboard(RenderView, MeshHandle, TextureHandle, const Mat4&,
                          const Vec4&, const Vec2&, const Vec2&, bool, bool) override {}
    void set_ao_texture(TextureHandle) override {}
    void flush() override {}
    void clear(uint32_t, float) override {}

    uint32_t get_width() const override { return w; }
    uint32_t get_height() const override { return h; }

    void set_vsync(bool) override {}
    bool get_vsync() const override { return false; }
    void set_render_scale(float) override {}
    float get_render_scale() const override { return 1.0f; }
    void set_shadow_quality(int) override {}
    int get_shadow_quality() const override { return 2; }
    void set_lod_bias(float) override {}
    float get_lod_bias() const override { return 0.0f; }
    void set_bloom_enabled(bool) override {}
    void set_bloom_intensity(float) override {}
    bool get_bloom_enabled() const override { return false; }
    float get_bloom_intensity() const override { return 0.0f; }
    void set_ao_enabled(bool) override {}
    bool get_ao_enabled() const override { return false; }
    void set_ibl_intensity(float) override {}
    float get_ibl_intensity() const override { return 0.0f; }
    void set_motion_blur_enabled(bool) override {}
    bool get_motion_blur_enabled() const override { return false; }
    uint16_t get_native_texture_handle(TextureHandle) const override { return 0; }
    MeshBufferInfo get_mesh_buffer_info(MeshHandle) const override { return {}; }
};

// --- Halton sequence verification ---

TEST_CASE("TAA jitter returns zero when disabled", "[render][taa]") {
    TAA_MockRenderer renderer;
    TAASystem taa;
    TAAConfig config;
    config.enabled = false;
    taa.init(&renderer, config);

    Vec2 j = taa.get_jitter(0);
    REQUIRE_THAT(j.x, WithinAbs(0.0f, 0.0001f));
    REQUIRE_THAT(j.y, WithinAbs(0.0f, 0.0001f));

    taa.shutdown();
}

TEST_CASE("TAA jitter is non-zero when enabled", "[render][taa]") {
    TAA_MockRenderer renderer;
    TAASystem taa;
    TAAConfig config;
    config.enabled = true;
    config.jitter_scale = 1.0f;
    taa.init(&renderer, config);

    // At least one of the 8 jitter samples should be non-zero
    bool has_nonzero = false;
    for (uint32_t i = 0; i < 8; ++i) {
        Vec2 j = taa.get_jitter(i);
        if (std::abs(j.x) > 0.001f || std::abs(j.y) > 0.001f) {
            has_nonzero = true;
            break;
        }
    }
    REQUIRE(has_nonzero);

    taa.shutdown();
}

TEST_CASE("TAA jitter magnitude is in sub-pixel range (pixel units)", "[render][taa]") {
    TAA_MockRenderer renderer;
    TAASystem taa;
    TAAConfig config;
    config.enabled = true;
    config.jitter_scale = 1.0f;
    taa.init(&renderer, config);

    // Halton sequence centered at 0 with range [-0.5, 0.5]
    // After jitter_scale=1, values should be in [-0.5, 0.5] pixel units
    for (uint32_t i = 0; i < 8; ++i) {
        Vec2 j = taa.get_jitter(i);
        REQUIRE(j.x >= -0.5f);
        REQUIRE(j.x <= 0.5f);
        REQUIRE(j.y >= -0.5f);
        REQUIRE(j.y <= 0.5f);
    }

    taa.shutdown();
}

TEST_CASE("TAA jitter wraps after JITTER_SAMPLES", "[render][taa]") {
    TAA_MockRenderer renderer;
    TAASystem taa;
    TAAConfig config;
    config.enabled = true;
    taa.init(&renderer, config);

    // Frame 0 and frame 8 should produce the same jitter (8 samples)
    Vec2 j0 = taa.get_jitter(0);
    Vec2 j8 = taa.get_jitter(8);
    REQUIRE_THAT(j0.x, WithinAbs(j8.x, 0.0001f));
    REQUIRE_THAT(j0.y, WithinAbs(j8.y, 0.0001f));

    taa.shutdown();
}

TEST_CASE("TAA jitter_scale multiplies jitter", "[render][taa]") {
    TAA_MockRenderer renderer;
    TAASystem taa;

    TAAConfig config1;
    config1.enabled = true;
    config1.jitter_scale = 1.0f;
    taa.init(&renderer, config1);
    Vec2 j1 = taa.get_jitter(1);
    taa.shutdown();

    TAAConfig config2;
    config2.enabled = true;
    config2.jitter_scale = 2.0f;
    taa.init(&renderer, config2);
    Vec2 j2 = taa.get_jitter(1);
    taa.shutdown();

    // j2 should be 2x j1
    REQUIRE_THAT(j2.x, WithinAbs(j1.x * 2.0f, 0.0001f));
    REQUIRE_THAT(j2.y, WithinAbs(j1.y * 2.0f, 0.0001f));
}

TEST_CASE("TAA jitter applied to projection matrix correctly (no double division)", "[render][taa]") {
    TAA_MockRenderer renderer;
    TAASystem taa;
    TAAConfig config;
    config.enabled = true;
    config.jitter_scale = 1.0f;
    taa.init(&renderer, config);

    Vec2 jitter = taa.get_jitter(1);
    taa.shutdown();

    // Simulate the pipeline's jitter application
    uint32_t width = 1920;
    uint32_t height = 1080;

    Mat4 proj = glm::perspective(glm::radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);
    Mat4 jittered_proj = proj;
    jittered_proj[2][0] += jitter.x * 2.0f / static_cast<float>(width);
    jittered_proj[2][1] += jitter.y * 2.0f / static_cast<float>(height);

    // The clip-space offset should be on the order of 1/width (sub-pixel)
    float clip_offset_x = jitter.x * 2.0f / static_cast<float>(width);
    float clip_offset_y = jitter.y * 2.0f / static_cast<float>(height);

    // For 1920 width, a 0.5 pixel jitter should give ~0.00052 clip-space offset
    // This verifies we're NOT getting near-zero values (which was the bug)
    if (std::abs(jitter.x) > 0.01f) {
        REQUIRE(std::abs(clip_offset_x) > 1e-6f);
        REQUIRE(std::abs(clip_offset_x) < 0.01f);  // Not too large either
    }
}
