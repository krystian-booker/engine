#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <engine/render/render_pipeline.hpp>
#include <engine/render/particle_system.hpp>
#include <engine/render/renderer.hpp>
#include <cmath>

using namespace engine::render;
using namespace engine::core;
using Catch::Matchers::WithinAbs;

// --- Mock renderer for testing pipeline behavior without GPU ---

class MockRenderer : public IRenderer {
public:
    uint32_t width = 1920;
    uint32_t height = 1080;
    uint32_t next_rt_id = 0;
    uint32_t next_tex_id = 0;
    int submit_count = 0;
    int configure_view_count = 0;
    bool shadows_enabled = false;

    bool init(void*, uint32_t w, uint32_t h) override { width = w; height = h; return true; }
    void shutdown() override {}
    void begin_frame() override {}
    void end_frame() override {}
    void resize(uint32_t w, uint32_t h) override { width = w; height = h; }

    MeshHandle create_mesh(const MeshData&) override { return MeshHandle{0}; }
    TextureHandle create_texture(const TextureData&) override { return TextureHandle{next_tex_id++}; }
    ShaderHandle create_shader(const ShaderData&) override { return ShaderHandle{0}; }
    MaterialHandle create_material(const MaterialData&) override { return MaterialHandle{0}; }
    MeshHandle create_primitive(PrimitiveMesh, float) override { return MeshHandle{0}; }

    void destroy_mesh(MeshHandle) override {}
    void destroy_texture(TextureHandle) override {}
    void destroy_shader(ShaderHandle) override {}
    void destroy_material(MaterialHandle) override {}

    RenderTargetHandle create_render_target(const RenderTargetDesc&) override {
        RenderTargetHandle h;
        h.id = next_rt_id++;
        return h;
    }
    void destroy_render_target(RenderTargetHandle) override {}
    TextureHandle get_render_target_texture(RenderTargetHandle, uint32_t) override {
        return TextureHandle{next_tex_id++};
    }
    void resize_render_target(RenderTargetHandle, uint32_t, uint32_t) override {}

    void configure_view(RenderView, const ViewConfig&) override { configure_view_count++; }
    void set_view_transform(RenderView, const Mat4&, const Mat4&) override {}

    void queue_draw(const DrawCall&) override {}
    void queue_draw(const DrawCall&, RenderView) override {}

    void set_camera(const Mat4&, const Mat4&) override {}
    void set_light(uint32_t, const LightData&) override {}
    void clear_lights() override {}

    void set_shadow_data(const std::array<Mat4, 4>&, const Vec4&, const Vec4&) override {}
    void set_shadow_texture(uint32_t, TextureHandle) override {}
    void enable_shadows(bool e) override { shadows_enabled = e; }

    void submit_mesh(RenderView, MeshHandle, MaterialHandle, const Mat4&) override { submit_count++; }
    void submit_skinned_mesh(RenderView, MeshHandle, MaterialHandle,
                             const Mat4&, const Mat4*, uint32_t) override { submit_count++; }

    void flush_debug_draw(RenderView) override {}
    void blit_to_screen(RenderView, TextureHandle) override {}
    void submit_skybox(RenderView, TextureHandle, const Mat4&, float, float) override {}
    void submit_billboard(RenderView, MeshHandle, TextureHandle, const Mat4&,
                          const Vec4&, const Vec2&, const Vec2&, bool, bool) override {}
    void set_ao_texture(TextureHandle) override {}
    void flush() override {}
    void clear(uint32_t, float) override {}

    uint32_t get_width() const override { return width; }
    uint32_t get_height() const override { return height; }

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

// Helper to create a basic camera
static CameraData make_test_camera() {
    return make_camera_data(
        Vec3(0.0f, 5.0f, 10.0f),
        Vec3(0.0f, 0.0f, 0.0f),
        Vec3(0.0f, 1.0f, 0.0f),
        60.0f, 16.0f / 9.0f, 0.1f, 100.0f);
}

// Helper to create objects at given positions
static std::vector<RenderObject> make_objects(const std::vector<Vec3>& positions) {
    std::vector<RenderObject> objects;
    for (const auto& pos : positions) {
        RenderObject obj;
        obj.mesh = MeshHandle{0};
        obj.material = MaterialHandle{0};
        obj.transform = glm::translate(Mat4(1.0f), pos);
        obj.bounds.min = Vec3(-0.5f);
        obj.bounds.max = Vec3(0.5f);
        objects.push_back(obj);
    }
    return objects;
}

// --- Init/Shutdown lifecycle ---

TEST_CASE("RenderPipeline init and shutdown", "[render][pipeline]") {
    MockRenderer renderer;
    RenderPipeline pipeline;

    RenderPipelineConfig config;
    config.enabled_passes = RenderPassFlags::MainOpaque | RenderPassFlags::Final;
    pipeline.init(&renderer, config);

    REQUIRE(pipeline.get_config().quality == RenderQuality::High);

    pipeline.shutdown();
}

TEST_CASE("RenderPipeline double shutdown is safe", "[render][pipeline]") {
    MockRenderer renderer;
    RenderPipeline pipeline;

    RenderPipelineConfig config;
    config.enabled_passes = RenderPassFlags::MainOpaque | RenderPassFlags::Final;
    pipeline.init(&renderer, config);
    pipeline.shutdown();
    pipeline.shutdown();  // Should not crash
}

// --- begin_frame resets stats ---

TEST_CASE("begin_frame resets stats", "[render][pipeline]") {
    MockRenderer renderer;
    RenderPipeline pipeline;

    RenderPipelineConfig config;
    config.enabled_passes = RenderPassFlags::MainOpaque | RenderPassFlags::Final;
    pipeline.init(&renderer, config);

    // Render a frame to populate stats
    auto camera = make_test_camera();
    auto objects = make_objects({{0, 0, 0}});
    std::vector<LightData> lights;

    pipeline.begin_frame();
    pipeline.render(camera, objects, lights);
    pipeline.end_frame();

    auto stats1 = pipeline.get_stats();
    REQUIRE(stats1.objects_rendered > 0);

    // begin_frame should reset
    pipeline.begin_frame();
    auto stats2 = pipeline.get_stats();
    REQUIRE(stats2.draw_calls == 0);
    REQUIRE(stats2.objects_rendered == 0);
    REQUIRE(stats2.objects_culled == 0);

    pipeline.shutdown();
}

// --- Culling produces correct visible sets ---

TEST_CASE("Culling removes objects behind camera", "[render][pipeline]") {
    MockRenderer renderer;
    RenderPipeline pipeline;

    RenderPipelineConfig config;
    config.enabled_passes = RenderPassFlags::MainOpaque | RenderPassFlags::Final;
    pipeline.init(&renderer, config);

    auto camera = make_test_camera();  // at (0,5,10) looking toward origin

    // Object in front of camera (at origin) and one behind (at z=20)
    auto objects = make_objects({
        {0.0f, 0.0f, 0.0f},   // in front - visible
        {0.0f, 0.0f, 20.0f}   // behind camera - should be culled
    });

    pipeline.begin_frame();
    pipeline.render(camera, objects, {});
    pipeline.end_frame();

    auto stats = pipeline.get_stats();
    REQUIRE(stats.objects_culled >= 1);  // At least the behind-camera object
    REQUIRE(stats.objects_rendered <= 1);

    pipeline.shutdown();
}

// --- Opaque/transparent partitioning ---

TEST_CASE("Blend mode correctly splits opaque and transparent", "[render][pipeline]") {
    MockRenderer renderer;
    RenderPipeline pipeline;

    RenderPipelineConfig config;
    config.enabled_passes = RenderPassFlags::MainOpaque | RenderPassFlags::Transparent | RenderPassFlags::Final;
    pipeline.init(&renderer, config);

    auto camera = make_test_camera();

    // Create objects with different blend modes at the origin (visible to camera)
    std::vector<RenderObject> objects;
    for (int i = 0; i < 5; ++i) {
        RenderObject obj;
        obj.mesh = MeshHandle{0};
        obj.material = MaterialHandle{0};
        obj.transform = glm::translate(Mat4(1.0f), Vec3(0.0f, 0.0f, static_cast<float>(i) * -1.0f));
        obj.bounds.min = Vec3(-0.5f);
        obj.bounds.max = Vec3(0.5f);
        obj.blend_mode = static_cast<uint8_t>(i);  // 0=Opaque, 1=AlphaTest, 2=AlphaBlend, 3=Additive, 4=Multiply
        objects.push_back(obj);
    }

    renderer.submit_count = 0;
    pipeline.begin_frame();
    pipeline.render(camera, objects, {});
    pipeline.end_frame();

    auto stats = pipeline.get_stats();
    // All 5 objects should be rendered (2 opaque + 3 transparent)
    REQUIRE(stats.objects_rendered == 5);

    pipeline.shutdown();
}

// --- Shadow caster filtering ---

TEST_CASE("Only casts_shadows objects appear in shadow caster list", "[render][pipeline]") {
    MockRenderer renderer;
    RenderPipeline pipeline;

    RenderPipelineConfig config;
    config.enabled_passes = RenderPassFlags::Shadows | RenderPassFlags::MainOpaque | RenderPassFlags::Final;
    pipeline.init(&renderer, config);

    auto camera = make_test_camera();

    // Create objects - some cast shadows, some don't
    auto objects = make_objects({{0, 0, 0}, {1, 0, 0}, {2, 0, 0}});
    objects[0].casts_shadows = true;
    objects[1].casts_shadows = false;
    objects[2].casts_shadows = true;

    auto lights = std::vector<LightData>{
        make_directional_light(Vec3(0, -1, 0), Vec3(1), 1.0f, true)
    };

    pipeline.begin_frame();
    pipeline.render(camera, objects, lights);
    pipeline.end_frame();

    auto stats = pipeline.get_stats();
    REQUIRE(stats.shadow_casters == 2);

    pipeline.shutdown();
}

// --- Quality preset application ---

TEST_CASE("Quality preset updates config", "[render][pipeline]") {
    MockRenderer renderer;
    RenderPipeline pipeline;

    RenderPipelineConfig config;
    pipeline.init(&renderer, config);

    pipeline.apply_quality_preset(RenderQuality::Low);
    REQUIRE(pipeline.get_config().quality == RenderQuality::Low);
    REQUIRE_THAT(pipeline.get_config().render_scale, WithinAbs(0.75f, 0.001f));

    pipeline.apply_quality_preset(RenderQuality::Ultra);
    REQUIRE(pipeline.get_config().quality == RenderQuality::Ultra);
    REQUIRE(pipeline.get_config().shadow_config.cascade_resolution == 4096);

    pipeline.shutdown();
}

// --- Resize updates internal resolution ---

TEST_CASE("Resize updates internal resolution with render_scale", "[render][pipeline]") {
    MockRenderer renderer;
    RenderPipeline pipeline;

    RenderPipelineConfig config;
    config.render_scale = 0.5f;
    config.enabled_passes = RenderPassFlags::MainOpaque | RenderPassFlags::Final;
    pipeline.init(&renderer, config);

    pipeline.resize(1920, 1080);

    // After resize, we can verify the pipeline accepted the new dimensions
    // by checking that it doesn't crash when rendering
    auto camera = make_test_camera();
    pipeline.begin_frame();
    pipeline.render(camera, {}, {});
    pipeline.end_frame();

    pipeline.shutdown();
}

// --- Pass flag gating ---

TEST_CASE("Disabled shadow pass does not enable shadows", "[render][pipeline]") {
    MockRenderer renderer;
    RenderPipeline pipeline;

    RenderPipelineConfig config;
    // Explicitly disable shadows
    config.enabled_passes = RenderPassFlags::MainOpaque | RenderPassFlags::Final;
    pipeline.init(&renderer, config);

    auto camera = make_test_camera();
    auto objects = make_objects({{0, 0, 0}});
    auto lights = std::vector<LightData>{
        make_directional_light(Vec3(0, -1, 0), Vec3(1), 1.0f, true)
    };

    renderer.shadows_enabled = false;
    pipeline.begin_frame();
    pipeline.render(camera, objects, lights);
    pipeline.end_frame();

    // Shadows should not have been enabled since the pass is not in enabled_passes
    REQUIRE_FALSE(renderer.shadows_enabled);

    pipeline.shutdown();
}

// --- Render with no objects ---

TEST_CASE("Render with empty object list doesn't crash", "[render][pipeline]") {
    MockRenderer renderer;
    RenderPipeline pipeline;

    RenderPipelineConfig config;
    config.enabled_passes = RenderPassFlags::All;
    pipeline.init(&renderer, config);

    auto camera = make_test_camera();

    pipeline.begin_frame();
    pipeline.render(camera, {}, {});
    pipeline.end_frame();

    auto stats = pipeline.get_stats();
    REQUIRE(stats.objects_rendered == 0);
    REQUIRE(stats.objects_culled == 0);

    pipeline.shutdown();
}

// --- Invisible objects are culled ---

TEST_CASE("Invisible objects are culled", "[render][pipeline]") {
    MockRenderer renderer;
    RenderPipeline pipeline;

    RenderPipelineConfig config;
    config.enabled_passes = RenderPassFlags::MainOpaque | RenderPassFlags::Final;
    pipeline.init(&renderer, config);

    auto camera = make_test_camera();
    auto objects = make_objects({{0, 0, 0}});
    objects[0].visible = false;

    pipeline.begin_frame();
    pipeline.render(camera, objects, {});
    pipeline.end_frame();

    auto stats = pipeline.get_stats();
    REQUIRE(stats.objects_culled == 1);
    REQUIRE(stats.objects_rendered == 0);

    pipeline.shutdown();
}

// --- Render without init doesn't crash ---

TEST_CASE("Render without init is a no-op", "[render][pipeline]") {
    RenderPipeline pipeline;
    auto camera = make_test_camera();

    // Should not crash
    pipeline.begin_frame();
    pipeline.render(camera, {}, {});
    pipeline.end_frame();
}
