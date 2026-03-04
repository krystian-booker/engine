#include <engine/core/application.hpp>
#include <engine/core/log.hpp>
#include <engine/core/math.hpp>
#include <engine/scene/scene.hpp>
#include <engine/scene/transform.hpp>
#include <engine/render/renderer.hpp>
#include <engine/render/render_pipeline.hpp>
#include <engine/core/filesystem.hpp>

#include <cmath>

using namespace engine::core;
using namespace engine::scene;
using namespace engine::render;

static constexpr int NUM_MODES = 4;
static constexpr float MODE_CYCLE_SEC = 4.0f;
static constexpr int NUM_SPHERES = 5;

static const char* MODE_NAMES[] = {
    "Full PBR + Shadows",
    "Shadow Map Debug",
    "Binary Shadows",
    "PCF Softness Test"
};

class ShadowValidationSample : public Application {
protected:
    void on_init() override {
        log(LogLevel::Info, "Shadow Validation Initializing...");

        auto* renderer = get_renderer();
        auto* world = get_world();
        auto* pipeline = get_render_pipeline();
        if (!renderer || !world || !pipeline) return;

        // --- Pipeline config with shadows enabled ---
        RenderPipelineConfig config;
        config.quality = RenderQuality::High;
        config.enabled_passes = RenderPassFlags::Shadows
            | RenderPassFlags::DepthPrepass
            | RenderPassFlags::MainOpaque
            | RenderPassFlags::PostProcess
            | RenderPassFlags::Final;
        config.show_debug_overlay = false;
        config.debug_view_mode = DebugViewMode::None;
        config.bloom_config.enabled = false;
        config.tonemap_config.op = ToneMappingOperator::ACES;
        config.tonemap_config.gamma = 2.2f;
        config.tonemap_config.exposure = 1.0f;
        config.shadow_config.cascade_count = 4;
        config.shadow_config.cascade_resolution = 2048;
        config.shadow_config.shadow_bias = 0.001f;
        config.shadow_config.normal_bias = 0.01f;
        config.shadow_config.pcf_enabled = true;
        pipeline->set_config(config);

        // --- Load shader (reuse existing vs_pbr vertex shader) ---
        std::string shader_path = renderer->get_shader_path();
        ShaderData shader_data;
        shader_data.vertex_binary = FileSystem::read_binary(shader_path + "vs_pbr.sc.bin");
        shader_data.fragment_binary = FileSystem::read_binary(shader_path + "fs_shadow_validation.sc.bin");
        m_shader = renderer->create_shader(shader_data);

        // --- Create meshes ---
        m_sphere_mesh = renderer->create_primitive(PrimitiveMesh::Sphere, 1.0f);
        m_plane_mesh = renderer->create_primitive(PrimitiveMesh::Cube, 1.0f);

        // --- Create materials for all modes ---
        // Sphere material: red albedo, dielectric, medium roughness
        // Ground material: gray albedo, dielectric, high roughness
        for (int m = 0; m < NUM_MODES; ++m) {
            // Sphere materials
            MaterialData sphere_mat;
            sphere_mat.shader = m_shader;
            sphere_mat.albedo = Vec4(0.8f, 0.15f, 0.1f, 1.0f); // Red
            sphere_mat.metallic = 0.0f;
            sphere_mat.roughness = 0.4f;
            sphere_mat.ao = 1.0f;
            sphere_mat.alpha_cutoff = static_cast<float>(m);
            m_sphere_materials[m] = renderer->create_material(sphere_mat);

            // Ground materials
            MaterialData ground_mat;
            ground_mat.shader = m_shader;
            ground_mat.albedo = Vec4(0.7f, 0.7f, 0.7f, 1.0f); // Gray
            ground_mat.metallic = 0.0f;
            ground_mat.roughness = 0.8f;
            ground_mat.ao = 1.0f;
            ground_mat.alpha_cutoff = static_cast<float>(m);
            m_ground_materials[m] = renderer->create_material(ground_mat);
        }

        // --- Spawn ground plane ---
        m_ground = world->create("Ground");
        world->registry().emplace<LocalTransform>(m_ground,
            Vec3(0.0f, 0.0f, 0.0f),
            Quat{1.0f, 0.0f, 0.0f, 0.0f},
            Vec3(20.0f, 1.0f, 20.0f));
        world->registry().emplace<WorldTransform>(m_ground);
        world->registry().emplace<PreviousTransform>(m_ground);
        world->registry().emplace<MeshRenderer>(m_ground, MeshRenderer{
            engine::scene::MeshHandle{m_plane_mesh.id},
            engine::scene::MaterialHandle{m_ground_materials[0].id},
            0, true, false, true  // visible=true, cast_shadows=false, receive_shadows=true
        });

        // --- Spawn spheres ---
        // Row 1: three spheres in a line
        Vec3 sphere_positions[] = {
            Vec3(-3.0f, 1.5f, 0.0f),
            Vec3( 0.0f, 1.5f, 0.0f),
            Vec3( 3.0f, 1.5f, 0.0f),
            // Row 2: two spheres offset for overlap shadows
            Vec3(-1.5f, 1.5f, -2.5f),
            Vec3( 1.5f, 1.5f, -2.5f)
        };

        for (int i = 0; i < NUM_SPHERES; ++i) {
            Entity ent = world->create("Sphere_" + std::to_string(i));
            world->registry().emplace<LocalTransform>(ent, sphere_positions[i]);
            world->registry().emplace<WorldTransform>(ent);
            world->registry().emplace<PreviousTransform>(ent);
            world->registry().emplace<MeshRenderer>(ent, MeshRenderer{
                engine::scene::MeshHandle{m_sphere_mesh.id},
                engine::scene::MaterialHandle{m_sphere_materials[0].id},
                0, true, true, true  // visible=true, cast_shadows=true, receive_shadows=true
            });
            m_spheres[i] = ent;
        }

        // --- Camera ---
        m_camera = world->create("Camera");
        Vec3 cam_pos(0.0f, 8.0f, 12.0f);
        Vec3 cam_target(0.0f, 0.0f, 0.0f);
        world->registry().emplace<LocalTransform>(m_camera, cam_pos,
            glm::quatLookAt(glm::normalize(cam_target - cam_pos), Vec3(0.0f, 1.0f, 0.0f)));
        world->registry().emplace<WorldTransform>(m_camera);

        Camera cam_comp;
        cam_comp.fov = 60.0f;
        cam_comp.aspect_ratio = static_cast<float>(window_width()) / static_cast<float>(window_height());
        cam_comp.near_plane = 0.1f;
        cam_comp.far_plane = 100.0f;
        cam_comp.active = true;
        world->registry().emplace<Camera>(m_camera, cam_comp);

        // Store default camera state for mode 3 restore
        m_default_cam_pos = cam_pos;
        m_default_cam_target = cam_target;
        m_default_cam_fov = cam_comp.fov;

        // --- Directional light (shadow-casting) ---
        m_light = world->create("Directional Light");
        Light l;
        l.type = LightType::Directional;
        l.color = Vec3(1.0f);
        l.intensity = 3.0f;
        l.cast_shadows = true;  // CRITICAL: enables shadow_pass()
        l.enabled = true;
        world->registry().emplace<Light>(m_light, l);

        world->registry().emplace<LocalTransform>(m_light, Vec3(0.0f),
            glm::quatLookAt(glm::normalize(Vec3(-1.0f, -0.7f, -1.0f)), Vec3(0.0f, 1.0f, 0.0f)));
        world->registry().emplace<WorldTransform>(m_light);

        log(LogLevel::Info, "Shadow Validation ready. Modes cycle every {:.1f} s.", MODE_CYCLE_SEC);
    }

    void on_update(double dt) override {
        auto* world = get_world();
        if (!world) return;

        engine::scene::transform_system(*world, dt);

        m_time += static_cast<float>(dt);

        // Rotate sun direction
        Vec3 sun_dir = glm::normalize(Vec3(-sinf(m_time * 0.3f), -0.7f, -cosf(m_time * 0.3f)));
        auto& lt = world->registry().get<LocalTransform>(m_light);
        lt.rotation = glm::quatLookAt(sun_dir, Vec3(0.0f, 1.0f, 0.0f));

        // Cycle modes every 4 seconds
        int current_mode = (static_cast<int>(m_time / MODE_CYCLE_SEC)) % NUM_MODES;
        if (current_mode != m_last_mode) {
            int prev_mode = m_last_mode;
            m_last_mode = current_mode;
            log(LogLevel::Info, "[Shadow Validation] Mode {}: {}", current_mode, MODE_NAMES[current_mode]);

            // Swap materials for all entities
            for (int i = 0; i < NUM_SPHERES; ++i) {
                auto* comp = world->registry().try_get<MeshRenderer>(m_spheres[i]);
                if (comp) {
                    comp->material = engine::scene::MaterialHandle{m_sphere_materials[current_mode].id};
                }
            }
            {
                auto* comp = world->registry().try_get<MeshRenderer>(m_ground);
                if (comp) {
                    comp->material = engine::scene::MaterialHandle{m_ground_materials[current_mode].id};
                }
            }

            // Mode 3: zoom camera to shadow edge for PCF softness test
            if (current_mode == 3) {
                auto& cam_lt = world->registry().get<LocalTransform>(m_camera);
                Vec3 zoom_pos(2.0f, 1.0f, 3.0f);
                Vec3 zoom_target(0.0f, 0.0f, 0.0f);
                cam_lt.position = zoom_pos;
                cam_lt.rotation = glm::quatLookAt(glm::normalize(zoom_target - zoom_pos), Vec3(0.0f, 1.0f, 0.0f));

                auto& cam = world->registry().get<Camera>(m_camera);
                cam.fov = 40.0f; // Narrow FOV for zoom
            }
            // Restore camera when leaving mode 3
            else if (prev_mode == 3) {
                auto& cam_lt = world->registry().get<LocalTransform>(m_camera);
                cam_lt.position = m_default_cam_pos;
                cam_lt.rotation = glm::quatLookAt(
                    glm::normalize(m_default_cam_target - m_default_cam_pos),
                    Vec3(0.0f, 1.0f, 0.0f));

                auto& cam = world->registry().get<Camera>(m_camera);
                cam.fov = m_default_cam_fov;
            }
        }

        // --- Auto-Capture Screenshots ---
        static const char* screenshot_names[] = {
            "shadow_validation_mode0_pbr_shadows.png",
            "shadow_validation_mode1_shadow_map_debug.png",
            "shadow_validation_mode2_binary_shadows.png",
            "shadow_validation_mode3_pcf_softness.png"
        };

        auto* renderer = get_renderer();
        auto* pipeline = get_render_pipeline();
        if (renderer && pipeline) {
            for (int i = 0; i < NUM_MODES; ++i) {
                float capture_time = 1.0f + i * MODE_CYCLE_SEC;
                if (m_time > capture_time && !m_captured[i]) {
                    m_captured[i] = true;
                    renderer->save_screenshot(screenshot_names[i], pipeline->get_final_texture());
                    log(LogLevel::Info, "Screenshot captured: {}", screenshot_names[i]);
                }
            }
        }

        // Quit after all modes have been shown and captured
        if (m_time > NUM_MODES * MODE_CYCLE_SEC + 1.0f) {
            quit();
        }
    }

    void on_shutdown() override {
        auto* renderer = get_renderer();
        if (!renderer) return;

        for (int m = 0; m < NUM_MODES; ++m) {
            if (m_sphere_materials[m].valid()) renderer->destroy_material(m_sphere_materials[m]);
            if (m_ground_materials[m].valid()) renderer->destroy_material(m_ground_materials[m]);
        }
        if (m_sphere_mesh.valid()) renderer->destroy_mesh(m_sphere_mesh);
        if (m_plane_mesh.valid()) renderer->destroy_mesh(m_plane_mesh);
    }

private:
    float m_time = 0.0f;
    int   m_last_mode = -1;

    // Render resources
    engine::render::MeshHandle    m_sphere_mesh;
    engine::render::MeshHandle    m_plane_mesh;
    engine::render::ShaderHandle  m_shader;
    engine::render::MaterialHandle m_sphere_materials[NUM_MODES];
    engine::render::MaterialHandle m_ground_materials[NUM_MODES];

    // Scene entities
    Entity m_spheres[NUM_SPHERES];
    Entity m_ground = NullEntity;
    Entity m_camera = NullEntity;
    Entity m_light = NullEntity;

    // Default camera state (for mode 3 restore)
    Vec3  m_default_cam_pos{0.0f};
    Vec3  m_default_cam_target{0.0f};
    float m_default_cam_fov = 60.0f;

    // Screenshot capture flags
    bool m_captured[NUM_MODES] = {};
};

#ifdef _WIN32
#include <windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    ShadowValidationSample app;
    return app.run(__argc, __argv);
}
#else
int main(int argc, char** argv) {
    ShadowValidationSample app;
    return app.run(argc, argv);
}
#endif
