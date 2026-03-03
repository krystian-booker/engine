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
static constexpr float MODE_CYCLE_SEC = 3.0f;

static const char* MODE_NAMES[] = {
    "Full PBR",
    "Specular Only",
    "Diffuse Only",
    "Fresnel Only"
};

class PbrAnalyticalFinalSample : public Application {
protected:
    void on_init() override {
        log(LogLevel::Info, "PBR Analytical Final Initializing...");

        auto* renderer = get_renderer();
        auto* world = get_world();
        auto* pipeline = get_render_pipeline();

        if (!renderer || !world || !pipeline) return;

        RenderPipelineConfig config;
        config.quality = RenderQuality::High;
        config.enabled_passes = RenderPassFlags::DepthPrepass
            | RenderPassFlags::MainOpaque
            | RenderPassFlags::PostProcess
            | RenderPassFlags::Final;
        config.show_debug_overlay = false;
        config.debug_view_mode = DebugViewMode::None;
        pipeline->set_config(config);

        std::string shader_path = renderer->get_shader_path();
        ShaderData shader_data;
        shader_data.vertex_binary = FileSystem::read_binary(shader_path + "vs_pbr_analytical_final.sc.bin");
        shader_data.fragment_binary = FileSystem::read_binary(shader_path + "fs_pbr_analytical_final.sc.bin");
        m_shader = renderer->create_shader(shader_data);

        m_sphere_mesh = renderer->create_primitive(PrimitiveMesh::Sphere, 1.0f);

        // Pre-create materials for all spheres and all modes
        for (int m = 0; m < NUM_MODES; ++m) {
            for (int row = 0; row < 2; ++row) {
                for (int col = 0; col < 5; ++col) {
                    float metallic = (row == 0) ? 0.0f : 1.0f;
                    float roughness = glm::mix(0.05f, 1.0f, static_cast<float>(col) / 4.0f);

                    MaterialData mat;
                    mat.shader = m_shader;
                    mat.albedo = Vec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
                    mat.metallic = metallic;
                    mat.roughness = roughness;
                    mat.ao = 1.0f;
                    mat.alpha_cutoff = static_cast<float>(m); 
                    
                    int index = row * 5 + col;
                    m_materials[m][index] = renderer->create_material(mat);
                }
            }
        }

        // Spawn spheres
        for (int row = 0; row < 2; ++row) {
            for (int col = 0; col < 5; ++col) {
                int index = row * 5 + col;
                Entity ent = world->create("Sphere_R" + std::to_string(row) + "_C" + std::to_string(col));
                
                float x = (col - 2.0f) * 2.5f;
                float y = (row - 0.5f) * -2.5f;
                
                world->registry().emplace<LocalTransform>(ent, Vec3(x, y, 0.0f));
                world->registry().emplace<WorldTransform>(ent);
                world->registry().emplace<PreviousTransform>(ent);
                world->registry().emplace<MeshRenderer>(ent, MeshRenderer{
                    engine::scene::MeshHandle{m_sphere_mesh.id},
                    engine::scene::MaterialHandle{m_materials[0][index].id},
                    0, true, false, false
                });
                m_spheres[index] = ent;
            }
        }

        m_camera = world->create("Camera");
        world->registry().emplace<LocalTransform>(m_camera, Vec3(0.0f, 0.0f, 10.0f),
            glm::quatLookAt(glm::normalize(Vec3(0.0f, 0.0f, 0.0f) - Vec3(0.0f, 0.0f, 10.0f)), Vec3(0.0f, 1.0f, 0.0f)));
        world->registry().emplace<WorldTransform>(m_camera);

        Camera cam_comp;
        cam_comp.fov = 60.0f;
        cam_comp.aspect_ratio = static_cast<float>(window_width()) / static_cast<float>(window_height());
        cam_comp.near_plane = 0.1f;
        cam_comp.far_plane = 100.0f;
        cam_comp.active = true;
        world->registry().emplace<Camera>(m_camera, cam_comp);

        m_light = world->create("Directional Light");
        Light l;
        l.type = LightType::Directional;
        l.color = Vec3(1.0f);
        l.intensity = 1.0f;
        l.cast_shadows = false;
        l.enabled = true;
        world->registry().emplace<Light>(m_light, l);

        world->registry().emplace<LocalTransform>(m_light, Vec3(0.0f),
            glm::quatLookAt(glm::normalize(Vec3(-1.0f, -0.5f, -1.0f)), Vec3(0.0f, 1.0f, 0.0f)));
        world->registry().emplace<WorldTransform>(m_light);

        log(LogLevel::Info, "PBR Analytical ready. Modes cycle every {:.1f} s.", MODE_CYCLE_SEC);
    }

    void on_update(double dt) override {
        auto* active_world = get_world();
        if (active_world) {
            engine::scene::transform_system(*active_world, dt);
        }

        m_time += static_cast<float>(dt);

        auto* world = get_world();
        if (world) {
            Vec3 dir(-sinf(m_time), -0.5f, -cosf(m_time));
            dir = glm::normalize(dir);
            auto& lt = world->registry().get<LocalTransform>(m_light);
            lt.rotation = glm::quatLookAt(dir, Vec3(0.0f, 1.0f, 0.0f));
        }

        int current_mode = (static_cast<int>(m_time / MODE_CYCLE_SEC)) % NUM_MODES;
        if (current_mode != m_last_mode) {
            m_last_mode = current_mode;
            log(LogLevel::Info, "[PBR Analytical Final] Mode {}: {}", current_mode, MODE_NAMES[current_mode]);

            if (world) {
                for (int i = 0; i < 10; ++i) {
                    auto* comp = world->registry().try_get<MeshRenderer>(m_spheres[i]);
                    if (comp) {
                        comp->material = engine::scene::MaterialHandle{ m_materials[current_mode][i].id };
                    }
                }
            }
        }
    }

    void on_shutdown() override {
        auto* renderer = get_renderer();
        if (!renderer) return;

        for (int m = 0; m < NUM_MODES; ++m) {
            for (int i = 0; i < 10; ++i) {
                if (m_materials[m][i].valid()) renderer->destroy_material(m_materials[m][i]);
            }
        }
        if (m_sphere_mesh.valid()) renderer->destroy_mesh(m_sphere_mesh);
    }

private:
    float m_time = 0.0f;
    int   m_last_mode = -1;

    engine::render::MeshHandle    m_sphere_mesh;
    engine::render::ShaderHandle  m_shader;
    engine::render::MaterialHandle m_materials[NUM_MODES][10];

    Entity m_spheres[10];
    Entity m_camera = NullEntity;
    Entity m_light = NullEntity;
};

#ifdef _WIN32
#include <windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    PbrAnalyticalFinalSample app;
    return app.run(__argc, __argv);
}
#else
int main(int argc, char** argv) {
    PbrAnalyticalFinalSample app;
    return app.run(argc, argv);
}
#endif
