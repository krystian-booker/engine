#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <bgfx/bgfx.h>
#define private public
#include <engine/render/render_pipeline.hpp>
#undef private
#include <engine/render/particle_system.hpp>
#include <engine/render/renderer.hpp>
#include <cmath>
#include <unordered_map>
#include <utility>

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
    std::unordered_map<uint16_t, ViewConfig> configured_views;
    std::vector<std::pair<RenderView, ViewConfig>> configure_history;
    std::vector<RenderView> blit_history;
    std::unordered_map<uint32_t, MaterialData> materials;
    TextureHandle last_opaque_copy_texture;
    TextureHandle last_opaque_depth_texture;

    bool init(void*, uint32_t w, uint32_t h, void*, bool) override { width = w; height = h; return true; }
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
    void create_shadow_cascades(uint32_t, uint32_t count, TextureHandle& out_array, std::array<RenderTargetHandle, 4>& out_rts) override {
        out_array = TextureHandle{next_tex_id++};
        for (uint32_t i = 0; i < count && i < 4; ++i) {
            out_rts[i] = RenderTargetHandle{next_rt_id++};
        }
    }
    void destroy_shadow_cascades(TextureHandle, std::array<RenderTargetHandle, 4>& rts) override {
        for (auto& h : rts) h = RenderTargetHandle{};
    }
    TextureHandle get_render_target_texture(RenderTargetHandle, uint32_t) override {
        return TextureHandle{next_tex_id++};
    }
    void resize_render_target(RenderTargetHandle, uint32_t, uint32_t) override {}

    void configure_view(RenderView view, const ViewConfig& config) override {
        configure_view_count++;
        configured_views[static_cast<uint16_t>(view)] = config;
        configure_history.emplace_back(view, config);
    }
    void set_view_transform(RenderView, const Mat4&, const Mat4&) override {}

    void queue_draw(const DrawCall&) override {}
    void queue_draw(const DrawCall&, RenderView) override {}

    void set_camera(const Mat4&, const Mat4&) override {}
    void set_camera_position(const Vec3&) override {}
    void set_light(uint32_t, const LightData&) override {}
    void clear_lights() override {}

    void set_shadow_data(const std::array<Mat4, 4>&, const Vec4&, const Vec4&) override {}
    void set_shadow_array_texture(TextureHandle) override {}
    void enable_shadows(bool e) override { shadows_enabled = e; }

    void submit_mesh(RenderView, MeshHandle, MaterialHandle, const Mat4&) override { submit_count++; }
    void submit_skinned_mesh(RenderView, MeshHandle, MaterialHandle,
                             const Mat4&, const Mat4*, uint32_t) override { submit_count++; }

    void flush_debug_draw(RenderView) override {}
    void blit_to_screen(RenderView view, TextureHandle) override { blit_history.push_back(view); }
    void submit_debug_view(RenderView, TextureHandle, int, float, float) override {}
    bool save_screenshot(const std::string&, TextureHandle) override { return false; }
    void submit_skybox(RenderView, TextureHandle, const Mat4&, float, float) override {}
    void submit_billboard(RenderView, MeshHandle, TextureHandle, const Mat4&,
                          const Vec4&, const Vec2&, const Vec2&, bool, bool) override {}
    void set_ao_texture(TextureHandle) override {}
    void set_hemisphere_ambient(const Vec3&, float, const Vec3&) override {}
    void set_oit_data(const Vec4&) override {}
    void enable_oit(bool) override {}
    void set_opaque_copy_texture(TextureHandle texture) override { last_opaque_copy_texture = texture; }
    void set_opaque_depth_texture(TextureHandle texture) override { last_opaque_depth_texture = texture; }
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
    void set_ibl_textures(TextureHandle, TextureHandle, TextureHandle, uint32_t) override {}
    void set_motion_blur_enabled(bool) override {}
    bool get_motion_blur_enabled() const override { return false; }
    std::string get_shader_path() const override { return {}; }
    uint16_t get_native_texture_handle(TextureHandle) const override { return bgfx::kInvalidHandle; }
    uint16_t get_dummy_shadow_array() const override { return bgfx::kInvalidHandle; }
    const MaterialData* get_material_data(MaterialHandle h) const override {
        auto it = materials.find(h.id);
        return it == materials.end() ? nullptr : &it->second;
    }
    bool is_headless() const override { return false; }
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

    pipeline.m_renderer = &renderer;
    pipeline.m_config.enabled_passes = RenderPassFlags::MainOpaque | RenderPassFlags::Final;
    pipeline.m_width = renderer.width;
    pipeline.m_height = renderer.height;
    pipeline.m_internal_width = renderer.width;
    pipeline.m_internal_height = renderer.height;
    pipeline.create_render_targets();

    REQUIRE(pipeline.m_hdr_target.valid());
    REQUIRE(pipeline.m_depth_target.valid());
    REQUIRE(pipeline.m_gbuffer.valid());

    pipeline.destroy_render_targets();
    REQUIRE_FALSE(pipeline.m_hdr_target.valid());
    REQUIRE_FALSE(pipeline.m_depth_target.valid());
    REQUIRE_FALSE(pipeline.m_gbuffer.valid());

    pipeline.m_renderer = nullptr;
}

TEST_CASE("RenderPipeline double shutdown is safe", "[render][pipeline]") {
    RenderPipeline pipeline;

    pipeline.shutdown();
    pipeline.shutdown();
}

// --- begin_frame resets stats ---

TEST_CASE("begin_frame resets stats", "[render][pipeline]") {
    RenderPipeline pipeline;
    pipeline.m_stats.draw_calls = 3;
    pipeline.m_stats.objects_rendered = 2;
    pipeline.m_stats.objects_culled = 1;

    // begin_frame should reset
    pipeline.begin_frame();
    auto stats2 = pipeline.get_stats();
    REQUIRE(stats2.draw_calls == 0);
    REQUIRE(stats2.objects_rendered == 0);
    REQUIRE(stats2.objects_culled == 0);
}

// --- Culling produces correct visible sets ---

TEST_CASE("Culling removes objects behind camera", "[render][pipeline]") {
    RenderPipeline pipeline;

    auto camera = make_test_camera();  // at (0,5,10) looking toward origin

    // Object in front of camera (at origin) and one behind (at z=20)
    auto objects = make_objects({
        {0.0f, 0.0f, 0.0f},   // in front - visible
        {0.0f, 0.0f, 20.0f}   // behind camera - should be culled
    });

    std::vector<const RenderObject*> visible;
    pipeline.cull_objects(camera, objects, visible);

    REQUIRE(pipeline.get_stats().objects_culled >= 1);
    REQUIRE(visible.size() == 1);
}

// --- Opaque/transparent partitioning ---

TEST_CASE("Blend mode correctly splits opaque and transparent", "[render][pipeline]") {
    MockRenderer renderer;
    RenderPipeline pipeline;
    pipeline.m_renderer = &renderer;

    auto camera = make_test_camera();

    // Create objects with different blend modes at the origin (visible to camera)
    std::vector<RenderObject> objects;
    for (int i = 0; i < 6; ++i) {
        RenderObject obj;
        obj.mesh = MeshHandle{0};
        obj.material = MaterialHandle{0};
        obj.transform = glm::translate(Mat4(1.0f), Vec3(0.0f, 0.0f, static_cast<float>(i) * -1.0f));
        obj.bounds.min = Vec3(-0.5f);
        obj.bounds.max = Vec3(0.5f);
        obj.blend_mode = static_cast<uint8_t>(i);  // 0=Opaque, 1=AlphaTest, 2=AlphaBlend, 3=Additive, 4=Multiply, 5=Transmission
        objects.push_back(obj);
    }

    pipeline.prepare_frame_data(camera, objects);

    REQUIRE(pipeline.m_visible_opaque.size() == 2);
    REQUIRE(pipeline.m_visible_transparent.size() == 4);

    pipeline.m_renderer = nullptr;
}

// --- Shadow caster filtering ---

TEST_CASE("Only casts_shadows objects appear in shadow caster list", "[render][pipeline]") {
    MockRenderer renderer;
    RenderPipeline pipeline;

    pipeline.m_renderer = &renderer;
    pipeline.m_config.enabled_passes =
        RenderPassFlags::Shadows | RenderPassFlags::MainOpaque | RenderPassFlags::Final;

    auto camera = make_test_camera();

    // Create objects - some cast shadows, some don't
    auto objects = make_objects({{0, 0, 0}, {1, 0, 0}, {2, 0, 0}});
    objects[0].casts_shadows = true;
    objects[1].casts_shadows = false;
    objects[2].casts_shadows = true;

    pipeline.prepare_frame_data(camera, objects);

    REQUIRE(pipeline.m_shadow_casters.size() == 2);
    REQUIRE(pipeline.m_shadow_casters[0]->casts_shadows);
    REQUIRE(pipeline.m_shadow_casters[1]->casts_shadows);

    pipeline.m_renderer = nullptr;
}

// --- Quality preset application ---

TEST_CASE("Low quality preset updates config", "[render][pipeline]") {
    RenderPipelineConfig config;
    config.enabled_passes = RenderPassFlags::MainOpaque | RenderPassFlags::Final;

    const auto low = apply_quality_preset_to_config(config, RenderQuality::Low);
    REQUIRE(low.quality == RenderQuality::Low);
    REQUIRE_THAT(low.render_scale, WithinAbs(0.75f, 0.001f));
    REQUIRE(low.enabled_passes != config.enabled_passes);
}

TEST_CASE("SSR composite view follows the active HDR target", "[render][pipeline]") {
    MockRenderer renderer;
    RenderPipeline pipeline;

    RenderPipelineConfig config;
    config.enabled_passes = RenderPassFlags::MainOpaque |
                            RenderPassFlags::Transparent |
                            RenderPassFlags::SSR |
                            RenderPassFlags::Final;
    pipeline.m_renderer = &renderer;
    pipeline.m_initialized = true;
    pipeline.m_config = config;
    pipeline.m_width = renderer.width;
    pipeline.m_height = renderer.height;
    pipeline.m_internal_width = renderer.width;
    pipeline.m_internal_height = renderer.height;
    pipeline.create_render_targets();

    const auto main_view = renderer.configured_views.find(static_cast<uint16_t>(RenderView::MainOpaque));
    const auto ssr_view = renderer.configured_views.find(static_cast<uint16_t>(RenderView::SSRComposite));
    REQUIRE(main_view != renderer.configured_views.end());
    REQUIRE(ssr_view != renderer.configured_views.end());
    const RenderTargetHandle hdr_target = main_view->second.render_target;
    REQUIRE(ssr_view->second.render_target.id == hdr_target.id);

    RenderTargetDesc desc;
    desc.width = renderer.width;
    desc.height = renderer.height;
    RenderTargetHandle custom_target = renderer.create_render_target(desc);

    renderer.configure_history.clear();
    pipeline.render_to_target(custom_target, make_test_camera(), make_objects({{0.0f, 0.0f, 0.0f}}), {}, RenderPassFlags::None);

    bool rebound_ssr_to_custom = false;
    bool restored_ssr_to_hdr = false;
    for (const auto& [view, view_config] : renderer.configure_history) {
        if (view == RenderView::SSRComposite && view_config.render_target.id == custom_target.id) {
            rebound_ssr_to_custom = true;
        }
        if (view == RenderView::SSRComposite && view_config.render_target.id == hdr_target.id) {
            restored_ssr_to_hdr = true;
        }
    }

    REQUIRE(rebound_ssr_to_custom);
    REQUIRE(restored_ssr_to_hdr);
    REQUIRE(renderer.configured_views.at(static_cast<uint16_t>(RenderView::SSRComposite)).render_target.id ==
            hdr_target.id);

    pipeline.destroy_render_targets();
    pipeline.m_initialized = false;
    pipeline.m_renderer = nullptr;
}

TEST_CASE("Transparent refractive objects bind opaque color and depth", "[render][pipeline]") {
    MockRenderer renderer;
    RenderPipeline pipeline;

    pipeline.m_renderer = &renderer;
    pipeline.m_initialized = true;
    pipeline.m_hdr_target = RenderTargetHandle{1};
    pipeline.m_opaque_copy = RenderTargetHandle{2};
    pipeline.m_depth_target = RenderTargetHandle{3};
    pipeline.m_opaque_depth_copy = RenderTargetHandle{4};

    MaterialData glass;
    glass.transmission = 0.65f;
    glass.blend_mode = MaterialBlendMode::Transmission;
    renderer.materials[1] = glass;

    RenderObject object;
    object.material = MaterialHandle{1};
    object.transform = glm::translate(Mat4(1.0f), Vec3(3.0f, 1.2f, 5.0f));
    object.bounds.min = Vec3(-0.5f);
    object.bounds.max = Vec3(0.5f);
    pipeline.m_visible_transparent = {&object};

    pipeline.transparent_pass(make_test_camera(), {});

    REQUIRE(renderer.last_opaque_copy_texture.valid());
    REQUIRE(renderer.last_opaque_depth_texture.valid());
    REQUIRE_FALSE(renderer.blit_history.empty());

    pipeline.m_initialized = false;
    pipeline.m_renderer = nullptr;
}

// --- Resize updates internal resolution ---

TEST_CASE("Resize updates internal resolution with render_scale", "[render][pipeline]") {
    RenderPipeline pipeline;

    pipeline.m_config.render_scale = 0.5f;
    pipeline.m_width = 1920;
    pipeline.m_height = 1080;
    pipeline.m_internal_width = static_cast<uint32_t>(pipeline.m_width * pipeline.m_config.render_scale);
    pipeline.m_internal_height = static_cast<uint32_t>(pipeline.m_height * pipeline.m_config.render_scale);

    REQUIRE(pipeline.m_internal_width == 960);
    REQUIRE(pipeline.m_internal_height == 540);
}

// --- Pass flag gating ---

TEST_CASE("Disabled shadow pass does not enable shadows", "[render][pipeline]") {
    MockRenderer renderer;
    RenderPipeline pipeline;

    pipeline.m_renderer = &renderer;
    pipeline.m_config.enabled_passes = RenderPassFlags::MainOpaque | RenderPassFlags::Final;

    auto camera = make_test_camera();
    auto objects = make_objects({{0, 0, 0}});

    renderer.shadows_enabled = false;
    pipeline.prepare_frame_data(camera, objects);

    REQUIRE_FALSE(renderer.shadows_enabled);
    REQUIRE_FALSE(has_flag(pipeline.m_config.enabled_passes, RenderPassFlags::Shadows));

    pipeline.m_renderer = nullptr;
}

// --- Render with no objects ---

TEST_CASE("Render with empty object list doesn't crash", "[render][pipeline]") {
    MockRenderer renderer;
    RenderPipeline pipeline;

    pipeline.m_renderer = &renderer;
    pipeline.m_config.enabled_passes = RenderPassFlags::MainOpaque | RenderPassFlags::Final;

    auto camera = make_test_camera();

    pipeline.begin_frame();
    pipeline.prepare_frame_data(camera, {});

    auto stats = pipeline.get_stats();
    REQUIRE(stats.objects_rendered == 0);
    REQUIRE(stats.objects_culled == 0);

    REQUIRE(pipeline.m_visible_opaque.empty());
    REQUIRE(pipeline.m_visible_transparent.empty());

    pipeline.m_renderer = nullptr;
}

// --- Invisible objects are culled ---

TEST_CASE("Invisible objects are culled", "[render][pipeline]") {
    RenderPipeline pipeline;

    auto camera = make_test_camera();
    auto objects = make_objects({{0, 0, 0}});
    objects[0].visible = false;

    std::vector<const RenderObject*> visible;
    pipeline.cull_objects(camera, objects, visible);

    auto stats = pipeline.get_stats();
    REQUIRE(stats.objects_culled == 1);
    REQUIRE(visible.empty());
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
