



#include <engine/core/application.hpp>
#include <engine/core/log.hpp>
#include <engine/core/math.hpp>
#include <engine/scene/scene.hpp>
#include <engine/scene/transform.hpp>
#include <engine/render/renderer.hpp>
#include <engine/render/render_pipeline.hpp>
#include <engine/core/filesystem.hpp>

#include <array>

using namespace engine::core;
using namespace engine::scene;
using namespace engine::render;

// =============================================================================
// PBR Analytical Lighting Sample
// =============================================================================
// Validates Cook-Torrance BRDF with a single directional light.
//
// Scene layout:
//   Row 0 (top):    5 dielectric spheres (metallic=0), roughness 0.0 -> 1.0
//   Row 1 (bottom): 5 metallic spheres   (metallic=1), roughness 0.0 -> 1.0
//
// Debug modes cycle every 4 seconds (driven by shader via u_time.x):
//   0 - Full PBR         4 - Specular Only
//   1 - Fresnel Only     5 - Diffuse Only
//   2 - NDF Only
//   3 - Geometry Only
//
// ---- VALIDATION HARD RULES (check visually) ----
// Energy Conservation: No sphere should appear brighter than the light.
//   If it does, check kD = (1 - F) * (1 - metalness).
// Metallic Coloration: Metallic=1 specular is tinted by albedo (gold/copper).
//   Metallic=0 specular is pure white (F0=0.04).
// Horizon Handling: NdotL and NdotV clamped to 0 prevents light leaking.
//
// ---- WHAT TO WATCH FOR ----
// "Glowing Edge" Bug (Mode 1): If Fresnel edges aren't turning white,
//   the View Vector V = normalize(cameraPos - worldPos) is likely wrong.
// "Too Dark" Roughness (Mode 0): If spheres get extremely dark at
//   roughness=1.0, the Geometry term is over-darkening or dividing by zero.
// Energy Conservation: A white sphere at roughness=0 and roughness=1 should
//   reflect roughly the same total light — it just spreads out more on rough.
// =============================================================================

static constexpr int   SPHERES_PER_ROW = 5;
static constexpr int   NUM_ROWS        = 2;
static constexpr int   TOTAL_SPHERES   = SPHERES_PER_ROW * NUM_ROWS;
static constexpr float SPHERE_SPACING  = 2.5f;
static constexpr float ROW_SPACING     = 3.0f;
static constexpr float DEBUG_CYCLE_SEC = 4.0f;
static constexpr int   NUM_DEBUG_MODES = 9;

static const char* DEBUG_MODE_NAMES[] = {
    "Full PBR",
    "Fresnel Only (F)",
    "NDF Only (D)",
    "Geometry Only (G)",
    "Specular Only (D*G*F)",
    "Diffuse Only (Lambertian)",
    "SMOKE: Normals",
    "SMOKE: CameraPos",
    "SMOKE: WorldPos"
};

class PbrLightingSample : public Application {
protected:
    void on_init() override {
        log(LogLevel::Info, "PBR Lighting Sample Initializing...");

        auto* renderer = get_renderer();
        auto* world    = get_world();
        auto* pipeline = get_render_pipeline();

        if (!renderer || !world || !pipeline) return;

        // Configure pipeline: opaque pass only, no IBL/shadows/post-processing
        RenderPipelineConfig config;
        config.quality = RenderQuality::High;
        config.enabled_passes = RenderPassFlags::DepthPrepass
                              | RenderPassFlags::MainOpaque
                              | RenderPassFlags::PostProcess
                              | RenderPassFlags::Final;
        config.show_debug_overlay = false;
        config.debug_view_mode = DebugViewMode::None;
        pipeline->set_config(config);

        // Sphere mesh
        m_sphere_mesh = renderer->create_primitive(PrimitiveMesh::Sphere, 1.0f);

        // Load custom PBR debug shader
        std::string shader_path = renderer->get_shader_path();
        ShaderData pbr_debug_shader_data;
        pbr_debug_shader_data.vertex_binary   = FileSystem::read_binary(shader_path + "vs_pbr.sc.bin");
        pbr_debug_shader_data.fragment_binary  = FileSystem::read_binary(shader_path + "fs_pbr_debug.sc.bin");
        m_pbr_debug_shader = renderer->create_shader(pbr_debug_shader_data);

        if (!m_pbr_debug_shader.valid()) {
            return;
        }

        // Create materials for each sphere
        // Row 0: Metallic = 0.0 (dielectric), roughness varies
        // Row 1: Metallic = 1.0, roughness varies
        // Albedo: warm gold (0.95, 0.64, 0.37) — shows metallic coloration clearly
        Vec4 dielectric_albedo(0.95f, 0.95f, 0.95f, 1.0f);  // Near-white for dielectrics
        Vec4 metallic_albedo(0.95f, 0.64f, 0.37f, 1.0f);    // Gold for metals

        // Pre-allocate debug materials for all modes
        for (int m = 0; m < NUM_DEBUG_MODES; ++m) {
            MaterialData mat;
            mat.shader = m_pbr_debug_shader;
            mat.albedo = Vec4(0.95f, 0.64f, 0.37f, 1.0f); // Gold albedo
            mat.metallic = 1.0f;
            mat.roughness = 0.5f;
            mat.alpha_cutoff = static_cast<float>(m); // Used as debugMode selector
            m_all_debug_materials[m] = renderer->create_material(mat);
        }

        for (int row = 0; row < NUM_ROWS; ++row) {
            for (int col = 0; col < SPHERES_PER_ROW; ++col) {
                int idx = row * SPHERES_PER_ROW + col;
                float roughness = static_cast<float>(col) / static_cast<float>(SPHERES_PER_ROW - 1);
                float metallic  = static_cast<float>(row); // 0.0 or 1.0

                MaterialData mat;
                mat.shader    = m_pbr_debug_shader;
                mat.albedo    = (row == 0) ? dielectric_albedo : metallic_albedo;
                mat.metallic  = metallic;
                mat.roughness = roughness;
                mat.ao        = 1.0f;
                mat.alpha_cutoff = 0.0f; // Default mode 0

                m_materials[idx] = renderer->create_material(mat);
            }
        }

        // ---- Camera (static, facing the sphere grid) ----
        m_camera = world->create("Camera");
        float grid_center_x = (SPHERES_PER_ROW - 1) * SPHERE_SPACING * 0.5f;
        float grid_center_y = -ROW_SPACING * 0.5f;
        m_camera_pos = Vec3(grid_center_x, grid_center_y, 10.0f);

        world->registry().emplace<LocalTransform>(m_camera, m_camera_pos,
            glm::quatLookAt(glm::normalize(Vec3(grid_center_x, grid_center_y, 0.0f) - m_camera_pos),
                            Vec3(0.0f, 1.0f, 0.0f)));
        world->registry().emplace<WorldTransform>(m_camera);

        Camera cam;
        cam.fov          = 50.0f;
        cam.aspect_ratio = static_cast<float>(window_width()) / static_cast<float>(window_height());
        cam.near_plane   = 0.1f;
        cam.far_plane    = 100.0f;
        cam.active       = true;
        world->registry().emplace<Camera>(m_camera, cam);

        // Create light
        auto light_entity = world->create("Directional Light");
        Light l;
        l.type         = LightType::Directional;
        l.color        = Vec3(1.0f);
        l.intensity    = 1.0f;
        l.cast_shadows = false;
        l.enabled      = true;
        world->registry().emplace<Light>(light_entity, l);

        world->registry().emplace<LocalTransform>(light_entity, Vec3(0.0f), 
            glm::quatLookAt(glm::normalize(Vec3(-1.0f, -1.0f, -1.0f)), Vec3(0.0f, 1.0f, 0.0f)));
        world->registry().emplace<WorldTransform>(light_entity);

        // ---- Create sphere entities ----
        for (int row = 0; row < NUM_ROWS; ++row) {
            for (int col = 0; col < SPHERES_PER_ROW; ++col) {
                int idx = row * SPHERES_PER_ROW + col;
                float x = static_cast<float>(col) * SPHERE_SPACING;
                float y = -static_cast<float>(row) * ROW_SPACING;

                std::string name = (row == 0 ? "Dielectric_R" : "Metallic_R")
                                 + std::to_string(col);
                m_spheres[idx] = world->create(name);

                world->registry().emplace<LocalTransform>(m_spheres[idx], Vec3(x, y, 0.0f));
                world->registry().emplace<WorldTransform>(m_spheres[idx]);
                world->registry().emplace<PreviousTransform>(m_spheres[idx]);
                world->registry().emplace<MeshRenderer>(m_spheres[idx], MeshRenderer{
                    engine::scene::MeshHandle{m_sphere_mesh.id},
                    engine::scene::MaterialHandle{m_materials[idx].id},
                    0, true, false, false
                });
            }
        }

        log(LogLevel::Info, "PBR Lighting Sample ready. Debug modes cycle every 4s.");
    }

    void on_update(double dt) override {
        auto* active_world = get_world();
        if (active_world) {
            engine::scene::transform_system(*active_world, dt);
        }

        // Enforce time advancement for headless capture
        m_time += (dt > 0.0) ? static_cast<float>(dt) : 0.1f;

        // Log mode transitions and update all material debug modes
        int current_mode = static_cast<int>(m_time / DEBUG_CYCLE_SEC) % NUM_DEBUG_MODES;
        if (current_mode != m_last_mode) {
            m_last_mode = current_mode;
            log(LogLevel::Info, "Debug Mode %d: %s", current_mode, DEBUG_MODE_NAMES[current_mode]);

            // Update all spheres with the material corresponding to this debug mode
            auto* renderer = get_renderer();
            auto* world = get_world();
            if (renderer && world) {
                for (int i = 0; i < TOTAL_SPHERES; ++i) {
                    engine::scene::MeshRenderer* comp = world->registry().try_get<MeshRenderer>(m_spheres[i]);
                    if (comp) {
                        comp->material = engine::scene::MaterialHandle{m_all_debug_materials[current_mode].id};
                    }
                }
            }
        }

        // Auto-capture screenshots for each mode, then quit
        auto* renderer = get_renderer();
        auto* pipeline = get_render_pipeline();
        if (renderer && pipeline) {
            for (int i = 0; i < NUM_DEBUG_MODES; ++i) {
                float capture_time = DEBUG_CYCLE_SEC * i + DEBUG_CYCLE_SEC * 0.5f;
                if (m_time > capture_time && !m_captured[i]) {
                    m_captured[i] = true;
                    std::string filename = "pbr_debug_mode" + std::to_string(i) + ".png";
                    renderer->save_screenshot(filename, pipeline->get_final_texture());
                    log(LogLevel::Info, "Captured: %s (%s)", filename.c_str(), DEBUG_MODE_NAMES[i]);
                }
            }

            // Quit after all modes captured plus a small buffer
            if (m_time > DEBUG_CYCLE_SEC * NUM_DEBUG_MODES + 2.0f) {
                quit();
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

    Vec3 m_camera_pos;

    engine::render::MeshHandle    m_sphere_mesh;
    engine::render::ShaderHandle  m_pbr_debug_shader;

    engine::render::MaterialHandle m_materials[TOTAL_SPHERES];
    engine::render::MaterialHandle m_all_debug_materials[NUM_DEBUG_MODES];
    Entity m_spheres[TOTAL_SPHERES];
    Entity m_camera = NullEntity;

    std::array<bool, NUM_DEBUG_MODES> m_captured{};
};

#ifdef _WIN32
#include <windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    PbrLightingSample app;
    return app.run(__argc, __argv);
}
#else
int main(int argc, char** argv) {
    PbrLightingSample app;
    return app.run(argc, argv);
}
#endif
