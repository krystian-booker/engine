#include <engine/core/application.hpp>
#include <engine/core/log.hpp>
#include <engine/core/math.hpp>
#include <engine/scene/scene.hpp>
#include <engine/scene/transform.hpp>
#include <engine/render/renderer.hpp>
#include <engine/render/render_pipeline.hpp>
#include <engine/core/filesystem.hpp>

using namespace engine::core;
using namespace engine::scene;
using namespace engine::render;

static constexpr int NUM_MODES = 4;
static constexpr float MODE_CYCLE_SEC = 4.0f;

static const char* MODE_NAMES[] = {
    "Clamped (ToneMappingOperator::None)",
    "Reinhard (ToneMappingOperator::ReinhardExtended)",
    "ACES (ToneMappingOperator::ACES)",
    "AgX (ToneMappingOperator::AgX)"
};

class TonemappingValidationSample : public Application {
protected:
    void on_init() override {
        log(LogLevel::Info, "Tonemapping Validation Initializing...");

        auto* renderer = get_renderer();
        auto* world = get_world();
        auto* pipeline = get_render_pipeline();

        if (!renderer || !world || !pipeline) return;

        // Base config flags
        RenderPipelineConfig config;
        config.quality = RenderQuality::High;
        config.enabled_passes = RenderPassFlags::DepthPrepass
            | RenderPassFlags::MainOpaque
            | RenderPassFlags::PostProcess
            | RenderPassFlags::Final;
        config.show_debug_overlay = false;
        config.debug_view_mode = DebugViewMode::None;
        
        // Disable bloom and vignette to truly validate just tonemapping
        config.bloom_config.enabled = false;
        config.tonemap_config.exposure = 1.0f;
        config.tonemap_config.gamma = 2.2f;
        config.tonemap_config.white_point = 4.0f; // High white point for Reinhard Extended

        pipeline->set_config(config);

        // We will use the standard PBR shader with a gold/yellow albedo material
        // We aren't testing PBR correctness here, we are testing the tonemapper
        // handling the very bright 10.0 directional light.
        std::string shader_path = renderer->get_shader_path();
        ShaderData shader_data;
        shader_data.vertex_binary = FileSystem::read_binary(shader_path + "vs_pbr.sc.bin");
        shader_data.fragment_binary = FileSystem::read_binary(shader_path + "fs_pbr.sc.bin");
        m_shader = renderer->create_shader(shader_data);

        m_sphere_mesh = renderer->create_primitive(PrimitiveMesh::Sphere, 1.0f);

        // Create standard material (Gold-like to see "bleed" in ACES)
        MaterialData mat;
        mat.shader = m_shader;
        mat.albedo = Vec4(1.0f, 0.8f, 0.4f, 1.0f); // Gold
        mat.metallic = 0.0f;
        mat.roughness = 0.3f;
        mat.ao = 1.0f;
        m_material = renderer->create_material(mat);

        // Create a single central sphere
        m_sphere = world->create("Center_Sphere");
        world->registry().emplace<LocalTransform>(m_sphere, Vec3(0.0f, 0.0f, 0.0f));
        world->registry().emplace<WorldTransform>(m_sphere);
        world->registry().emplace<PreviousTransform>(m_sphere);
        world->registry().emplace<MeshRenderer>(m_sphere, MeshRenderer{
            engine::scene::MeshHandle{m_sphere_mesh.id},
            engine::scene::MaterialHandle{m_material.id},
            0, true, false, false
        });

        // Set up camera
        m_camera = world->create("Camera");
        world->registry().emplace<LocalTransform>(m_camera, Vec3(0.0f, 0.0f, 5.0f),
            glm::quatLookAt(glm::normalize(Vec3(0.0f, 0.0f, 0.0f) - Vec3(0.0f, 0.0f, 5.0f)), Vec3(0.0f, 1.0f, 0.0f)));
        world->registry().emplace<WorldTransform>(m_camera);

        Camera cam_comp;
        cam_comp.fov = 60.0f;
        cam_comp.aspect_ratio = static_cast<float>(window_width()) / static_cast<float>(window_height());
        cam_comp.near_plane = 0.1f;
        cam_comp.far_plane = 100.0f;
        cam_comp.active = true;
        world->registry().emplace<Camera>(m_camera, cam_comp);

        // Set up "The Sun" (Directional Light Intensity 10.0)
        m_light = world->create("Directional Light");
        Light l;
        l.type = LightType::Directional;
        l.color = Vec3(1.0f);
        l.intensity = 10.0f; // IMPORTANT: The core of the validation
        l.cast_shadows = false;
        l.enabled = true;
        world->registry().emplace<Light>(m_light, l);

        world->registry().emplace<LocalTransform>(m_light, Vec3(0.0f),
            glm::quatLookAt(glm::normalize(Vec3(-1.0f, -0.5f, -1.0f)), Vec3(0.0f, 1.0f, 0.0f)));
        world->registry().emplace<WorldTransform>(m_light);

        log(LogLevel::Info, "Tonemapping Validation ready. Modes cycle every {:.1f} s.", MODE_CYCLE_SEC);
        
        apply_mode(0); // Start clamped
    }

    void apply_mode(int mode) {
        auto* pipeline = get_render_pipeline();
        if (!pipeline) return;
        
        RenderPipelineConfig config = pipeline->get_config();
        
        switch (mode) {
            case 0: config.tonemap_config.op = ToneMappingOperator::None; break;
            case 1: config.tonemap_config.op = ToneMappingOperator::ReinhardExtended; break;
            case 2: config.tonemap_config.op = ToneMappingOperator::ACES; break;
            case 3: config.tonemap_config.op = ToneMappingOperator::AgX; break;
        }
        
        pipeline->set_config(config);
    }

    void on_update(double dt) override {
        auto* active_world = get_world();
        if (active_world) {
            engine::scene::transform_system(*active_world, dt);
        }

        m_time += static_cast<float>(dt);

        int current_mode = (static_cast<int>(m_time / MODE_CYCLE_SEC)) % NUM_MODES;
        if (current_mode != m_last_mode) {
            m_last_mode = current_mode;
            log(LogLevel::Info, "[Tonemapping Validation] Mode {}: {}", current_mode, MODE_NAMES[current_mode]);
            apply_mode(current_mode);
        }
    }

    void on_shutdown() override {
        auto* renderer = get_renderer();
        if (!renderer) return;

        if (m_material.valid()) renderer->destroy_material(m_material);
        if (m_sphere_mesh.valid()) renderer->destroy_mesh(m_sphere_mesh);
    }

private:
    float m_time = 0.0f;
    int   m_last_mode = -1;

    engine::render::MeshHandle    m_sphere_mesh;
    engine::render::ShaderHandle  m_shader;
    engine::render::MaterialHandle m_material;

    Entity m_sphere = NullEntity;
    Entity m_camera = NullEntity;
    Entity m_light = NullEntity;
};

#ifdef _WIN32
#include <windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    TonemappingValidationSample app;
    return app.run(__argc, __argv);
}
#else
int main(int argc, char** argv) {
    TonemappingValidationSample app;
    return app.run(argc, argv);
}
#endif
