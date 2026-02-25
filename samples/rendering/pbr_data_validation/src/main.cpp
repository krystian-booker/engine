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

// =============================================================================
// PBR Data Validation Sample
// =============================================================================
// Validates that engine uniforms (camera position, light direction) reach the
// fragment shader correctly. Uses the standard PBR vertex shader and engine
// uniform pipeline — no raw bgfx calls.
//
// Debug modes cycle every 3 seconds (packed into MaterialData::alpha_cutoff):
//   0 - Normals        (rainbow sphere)
//   1 - N dot L        (lit hemisphere)
//   2 - N dot V        (bright center, dark edges — the key validation)
// =============================================================================

static constexpr int   NUM_MODES       = 3;
static constexpr float MODE_CYCLE_SEC  = 3.0f;

static const char* MODE_NAMES[] = {
    "Normals",
    "N dot L",
    "N dot V"
};

class PbrDataValidationSample : public Application {
protected:
    void on_init() override {
        log(LogLevel::Info, "PBR Data Validation Initializing...");

        auto* renderer = get_renderer();
        auto* world    = get_world();
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

        // Load shader: standard PBR vertex + data validation fragment
        std::string shader_path = renderer->get_shader_path();
        ShaderData shader_data;
        shader_data.vertex_binary   = FileSystem::read_binary(shader_path + "vs_pbr.sc.bin");
        shader_data.fragment_binary = FileSystem::read_binary(shader_path + "fs_pbr_data_validation.sc.bin");
        m_shader = renderer->create_shader(shader_data);

        if (!m_shader.valid()) {
            log(LogLevel::Error, "Failed to load validation shaders!");
            return;
        }

        // Sphere mesh
        m_sphere_mesh = renderer->create_primitive(PrimitiveMesh::Sphere, 2.0f);

        // Pre-create one material per debug mode (mode packed into alpha_cutoff → u_pbrParams.w)
        for (int m = 0; m < NUM_MODES; ++m) {
            MaterialData mat;
            mat.shader       = m_shader;
            mat.albedo       = Vec4(1.0f);
            mat.metallic     = 0.0f;
            mat.roughness    = 0.5f;
            mat.ao           = 1.0f;
            mat.alpha_cutoff = static_cast<float>(m);
            m_materials[m] = renderer->create_material(mat);
        }

        // Spawn test sphere
        m_sphere_ent = world->create("TestSphere");
        world->registry().emplace<LocalTransform>(m_sphere_ent, Vec3(0.0f, 0.0f, 0.0f));
        world->registry().emplace<WorldTransform>(m_sphere_ent);
        world->registry().emplace<PreviousTransform>(m_sphere_ent);
        world->registry().emplace<MeshRenderer>(m_sphere_ent, MeshRenderer{
            engine::scene::MeshHandle{m_sphere_mesh.id},
            engine::scene::MaterialHandle{m_materials[0].id},
            0, true, false, false
        });

        // Camera
        m_camera = world->create("Camera");
        world->registry().emplace<LocalTransform>(m_camera, Vec3(0.0f, 0.0f, 6.0f),
            glm::quatLookAt(glm::normalize(Vec3(0.0f, 0.0f, 0.0f) - Vec3(0.0f, 0.0f, 6.0f)), Vec3(0.0f, 1.0f, 0.0f)));
        world->registry().emplace<WorldTransform>(m_camera);

        Camera cam_comp;
        cam_comp.fov          = 60.0f;
        cam_comp.aspect_ratio = static_cast<float>(window_width()) / static_cast<float>(window_height());
        cam_comp.near_plane   = 0.1f;
        cam_comp.far_plane    = 100.0f;
        cam_comp.active       = true;
        world->registry().emplace<Camera>(m_camera, cam_comp);

        // Directional light entity (populates u_lights[] via engine light gather)
        m_light = world->create("Directional Light");
        Light l;
        l.type         = LightType::Directional;
        l.color        = Vec3(1.0f);
        l.intensity    = 1.0f;
        l.cast_shadows = false;
        l.enabled      = true;
        world->registry().emplace<Light>(m_light, l);

        world->registry().emplace<LocalTransform>(m_light, Vec3(0.0f),
            glm::quatLookAt(glm::normalize(Vec3(-1.0f, -0.5f, -1.0f)), Vec3(0.0f, 1.0f, 0.0f)));
        world->registry().emplace<WorldTransform>(m_light);

        log(LogLevel::Info, "PBR Data Validation ready. Modes cycle every 3s.");
    }

    void on_update(double dt) override {
        auto* active_world = get_world();
        if (active_world) {
            engine::scene::transform_system(*active_world, dt);
        }

        m_time += static_cast<float>(dt);

        // Animate light direction: rotate around Y axis (matches original rotating light)
        {
            auto* world = get_world();
            if (world) {
                Vec3 dir(-sinf(m_time), -0.5f, -cosf(m_time));
                dir = glm::normalize(dir);
                auto& lt = world->registry().get<LocalTransform>(m_light);
                lt.rotation = glm::quatLookAt(dir, Vec3(0.0f, 1.0f, 0.0f));
            }
        }

        int current_mode = (static_cast<int>(m_time / MODE_CYCLE_SEC)) % NUM_MODES;
        if (current_mode != m_last_mode) {
            m_last_mode = current_mode;
            log(LogLevel::Info, "[PBR Data Validation] Mode %d: %s", current_mode, MODE_NAMES[current_mode]);

            // Swap material on the sphere
            auto* world = get_world();
            if (world) {
                auto* comp = world->registry().try_get<MeshRenderer>(m_sphere_ent);
                if (comp) {
                    comp->material = engine::scene::MaterialHandle{m_materials[current_mode].id};
                }
            }
        }
    }

    void on_shutdown() override {
        auto* renderer = get_renderer();
        if (!renderer) return;

        for (auto& mat : m_materials) {
            if (mat.valid()) renderer->destroy_material(mat);
        }
        if (m_sphere_mesh.valid()) renderer->destroy_mesh(m_sphere_mesh);
    }

private:
    float m_time = 0.0f;
    int   m_last_mode = -1;

    engine::render::MeshHandle    m_sphere_mesh;
    engine::render::ShaderHandle  m_shader;
    engine::render::MaterialHandle m_materials[NUM_MODES];

    Entity m_sphere_ent = NullEntity;
    Entity m_camera     = NullEntity;
    Entity m_light      = NullEntity;
};

#ifdef _WIN32
#include <windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    PbrDataValidationSample app;
    return app.run(__argc, __argv);
}
#else
int main(int argc, char** argv) {
    PbrDataValidationSample app;
    return app.run(argc, argv);
}
#endif
