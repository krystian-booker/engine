// Render Test Scene
// Deterministic scene that exercises all major rendering features:
//   PBR materials, shadows, SSAO, bloom, transparency
// No input handling, no animation — purely static and deterministic.
// Usage: render_test.exe --screenshot=output.png --screenshot-frame=60

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <engine/core/application.hpp>
#include <engine/core/log.hpp>
#include <engine/scene/scene.hpp>
#include <engine/render/renderer.hpp>
#include <engine/render/render_pipeline.hpp>

using namespace engine::core;
using namespace engine::scene;
namespace render = engine::render;

class RenderTestApp : public Application {
protected:
    void on_init() override {
        log(LogLevel::Info, "[RenderTest] Initializing deterministic test scene...");

        auto* renderer = get_renderer();
        if (!renderer) {
            log(LogLevel::Error, "[RenderTest] Renderer not available");
            quit();
            return;
        }

        auto* world = get_world();
        if (!world) {
            log(LogLevel::Error, "[RenderTest] World not available");
            quit();
            return;
        }

        // Create primitive meshes
        m_sphere_mesh = renderer->create_primitive(render::PrimitiveMesh::Sphere, 1.0f);
        m_cube_mesh = renderer->create_primitive(render::PrimitiveMesh::Cube, 1.0f);
        m_plane_mesh = renderer->create_primitive(render::PrimitiveMesh::Plane, 1.0f);

        // Configure the render pipeline
        auto* pipeline = get_render_pipeline();
        if (pipeline) {
            render::RenderPipelineConfig config;
            config.enabled_passes = render::RenderPassFlags::Shadows
                                  | render::RenderPassFlags::DepthPrepass
                                  | render::RenderPassFlags::GBuffer
                                  | render::RenderPassFlags::SSAO
                                  | render::RenderPassFlags::MainOpaque
                                  | render::RenderPassFlags::Transparent
                                  | render::RenderPassFlags::PostProcess
                                  | render::RenderPassFlags::Final;

            // Tone mapping: ACES
            config.tonemap_config.op = render::ToneMappingOperator::ACES;
            config.tonemap_config.exposure = 1.0f;

            // Bloom
            config.bloom_config.enabled = true;
            config.bloom_config.threshold = 1.5f;
            config.bloom_config.intensity = 0.15f;

            // SSAO
            config.ssao_config.radius = 0.5f;
            config.ssao_config.intensity = 1.5f;

            // Shadows
            config.shadow_config.cascade_resolution = 2048;
            config.shadow_config.cascade_count = 4;

            config.clear_color = 0x1a1a2eFF;  // Dark blue-ish background

            pipeline->set_config(config);
        }

        renderer->set_ibl_intensity(1.0f);

        create_camera(world);
        create_lights(world, renderer);
        create_ground(world, renderer);
        create_pbr_sphere_grid(world, renderer);
        create_shadow_casters(world, renderer);
        create_emissive_sphere(world, renderer);
        create_ssao_corner(world, renderer);
        create_glass_sphere(world, renderer);

        log(LogLevel::Info, "[RenderTest] Scene initialized.");
    }

    void on_shutdown() override {
        log(LogLevel::Info, "[RenderTest] Shutting down...");

        if (auto* renderer = get_renderer()) {
            renderer->destroy_mesh(m_sphere_mesh);
            renderer->destroy_mesh(m_cube_mesh);
            renderer->destroy_mesh(m_plane_mesh);
            for (auto& mat : m_materials) {
                renderer->destroy_material(mat);
            }
        }
    }

private:
    // ---- Camera ----
    void create_camera(World* world) {
        auto cam_entity = world->create("Camera");
        auto& local_tf = world->emplace<LocalTransform>(cam_entity, Vec3{0.0f, 7.5f, 21.0f});
        local_tf.look_at(Vec3(0.0f, 1.0f, 0.0f));
        world->emplace<WorldTransform>(cam_entity);

        Camera cam;
        cam.fov = 32.0f;
        cam.aspect_ratio = static_cast<float>(window_width()) / static_cast<float>(window_height());
        cam.near_plane = 0.1f;
        cam.far_plane = 200.0f;
        cam.active = true;
        world->emplace<Camera>(cam_entity, cam);
    }

    // ---- Lights ----
    void create_lights(World* world, render::IRenderer* /*renderer*/) {
        // Sun light — warm white, casts shadows
        {
            auto entity = world->create("Sun");
            Vec3 dir = glm::normalize(Vec3{0.5f, -1.0f, 0.5f});
            Vec3 up = (std::abs(dir.y) > 0.99f) ? Vec3(0.0f, 0.0f, 1.0f) : Vec3(0.0f, 1.0f, 0.0f);
            Quat rot = glm::quatLookAt(dir, up);
            world->emplace<LocalTransform>(entity, Vec3{0.0f}, rot);
            world->emplace<WorldTransform>(entity);

            Light light;
            light.type = LightType::Directional;
            light.color = Vec3{1.0f, 0.95f, 0.9f};
            light.intensity = 2.0f;
            light.cast_shadows = true;
            light.enabled = true;
            world->emplace<Light>(entity, light);
        }

        // Fill light — cool blue, no shadows
        {
            auto entity = world->create("Fill");
            Vec3 dir = glm::normalize(Vec3{0.5f, -0.3f, 0.5f});
            Vec3 up = (std::abs(dir.y) > 0.99f) ? Vec3(0.0f, 0.0f, 1.0f) : Vec3(0.0f, 1.0f, 0.0f);
            Quat rot = glm::quatLookAt(dir, up);
            world->emplace<LocalTransform>(entity, Vec3{0.0f}, rot);
            world->emplace<WorldTransform>(entity);

            Light light;
            light.type = LightType::Directional;
            light.color = Vec3{0.6f, 0.7f, 1.0f};
            light.intensity = 0.3f;
            light.cast_shadows = false;
            light.enabled = true;
            world->emplace<Light>(entity, light);
        }
    }

    // ---- Ground Plane ----
    void create_ground(World* world, render::IRenderer* renderer) {
        render::MaterialData mat_data;
        mat_data.albedo = Vec4{0.5f, 0.5f, 0.52f, 1.0f};
        mat_data.roughness = 0.95f;
        mat_data.metallic = 0.0f;
        auto mat = renderer->create_material(mat_data);
        m_materials.push_back(mat);

        auto entity = world->create("Ground");
        auto& tf = world->emplace<LocalTransform>(entity, Vec3{0.0f, 0.0f, 0.0f});
        tf.scale = Vec3{20.0f, 0.1f, 20.0f};
        world->emplace<WorldTransform>(entity);
        world->emplace<PreviousTransform>(entity);
        world->emplace<MeshRenderer>(entity, MeshRenderer{
            MeshHandle{m_cube_mesh.id}, MaterialHandle{mat.id}, 0, true, false, true
        });
    }

    // ---- 5x5 PBR Sphere Grid ----
    // Metallic 0->1 across X, roughness 0.1->1.0 across Z
    void create_pbr_sphere_grid(World* world, render::IRenderer* renderer) {
        constexpr int GRID_SIZE = 5;
        constexpr float SPACING = 2.0f;
        constexpr float START_X = -(GRID_SIZE - 1) * SPACING * 0.5f;
        constexpr float START_Z = -(GRID_SIZE - 1) * SPACING * 0.5f;

        for (int ix = 0; ix < GRID_SIZE; ++ix) {
            for (int iz = 0; iz < GRID_SIZE; ++iz) {
                float metallic = static_cast<float>(ix) / static_cast<float>(GRID_SIZE - 1);
                float roughness = 0.1f + 0.9f * static_cast<float>(iz) / static_cast<float>(GRID_SIZE - 1);

                // Gold-ish albedo for metallic rows, neutral for dielectric
                Vec3 albedo = (metallic > 0.3f)
                    ? Vec3{1.0f, 0.86f, 0.57f}   // Gold
                    : Vec3{0.9f, 0.1f, 0.1f};     // Red dielectric

                render::MaterialData mat_data;
                mat_data.albedo = Vec4{albedo.x, albedo.y, albedo.z, 1.0f};
                mat_data.roughness = roughness;
                mat_data.metallic = metallic;
                auto mat = renderer->create_material(mat_data);
                m_materials.push_back(mat);

                float x = START_X + ix * SPACING;
                float z = START_Z + iz * SPACING;

                auto entity = world->create("PBRSphere_" + std::to_string(ix) + "_" + std::to_string(iz));
                auto& tf = world->emplace<LocalTransform>(entity, Vec3{x, 1.0f, z});
                tf.scale = Vec3{0.8f};
                world->emplace<WorldTransform>(entity);
                world->emplace<PreviousTransform>(entity);
                world->emplace<MeshRenderer>(entity, MeshRenderer{
                    MeshHandle{m_sphere_mesh.id}, MaterialHandle{mat.id}, 0, true, true, true
                });
            }
        }
    }

    // ---- Shadow Casters (tall cubes) ----
    void create_shadow_casters(World* world, render::IRenderer* renderer) {
        render::MaterialData mat_data;
        mat_data.albedo = Vec4{0.3f, 0.3f, 0.35f, 1.0f};
        mat_data.roughness = 0.6f;
        mat_data.metallic = 0.0f;
        auto mat = renderer->create_material(mat_data);
        m_materials.push_back(mat);

        Vec3 positions[] = {{6.0f, 2.0f, -2.0f}, {-6.0f, 2.0f, 1.0f}};
        for (int i = 0; i < 2; ++i) {
            auto entity = world->create("ShadowCube_" + std::to_string(i));
            auto& tf = world->emplace<LocalTransform>(entity, positions[i]);
            tf.scale = Vec3{1.0f, 4.0f, 1.0f};
            world->emplace<WorldTransform>(entity);
            world->emplace<PreviousTransform>(entity);
            world->emplace<MeshRenderer>(entity, MeshRenderer{
                MeshHandle{m_cube_mesh.id}, MaterialHandle{mat.id}, 0, true, true, true
            });
        }
    }

    // ---- Emissive Sphere (bloom test) ----
    void create_emissive_sphere(World* world, render::IRenderer* renderer) {
        render::MaterialData mat_data;
        mat_data.albedo = Vec4{1.0f, 0.3f, 0.1f, 1.0f};
        mat_data.roughness = 0.3f;
        mat_data.metallic = 0.0f;
        mat_data.emissive = Vec3{8.0f, 2.0f, 0.5f};
        auto mat = renderer->create_material(mat_data);
        m_materials.push_back(mat);

        auto entity = world->create("EmissiveSphere");
        auto& tf = world->emplace<LocalTransform>(entity, Vec3{6.0f, 1.5f, 2.0f});
        tf.scale = Vec3{1.0f};
        world->emplace<WorldTransform>(entity);
        world->emplace<PreviousTransform>(entity);
        world->emplace<MeshRenderer>(entity, MeshRenderer{
            MeshHandle{m_sphere_mesh.id}, MaterialHandle{mat.id}, 0, true, true, true
        });
    }

    // ---- SSAO Corner (concave crevice) ----
    void create_ssao_corner(World* world, render::IRenderer* renderer) {
        render::MaterialData mat_data;
        mat_data.albedo = Vec4{0.7f, 0.7f, 0.72f, 1.0f};
        mat_data.roughness = 0.9f;
        mat_data.metallic = 0.0f;
        auto mat = renderer->create_material(mat_data);
        m_materials.push_back(mat);

        // Large cube
        {
            auto entity = world->create("SSAOCubeLarge");
            auto& tf = world->emplace<LocalTransform>(entity, Vec3{-6.0f, 1.5f, -3.0f});
            tf.scale = Vec3{3.0f};
            world->emplace<WorldTransform>(entity);
            world->emplace<PreviousTransform>(entity);
            world->emplace<MeshRenderer>(entity, MeshRenderer{
                MeshHandle{m_cube_mesh.id}, MaterialHandle{mat.id}, 0, true, true, true
            });
        }
        // Small nested cube tucked in the corner
        {
            auto entity = world->create("SSAOCubeSmall");
            auto& tf = world->emplace<LocalTransform>(entity, Vec3{-4.8f, 0.4f, -1.8f});
            tf.scale = Vec3{0.8f};
            world->emplace<WorldTransform>(entity);
            world->emplace<PreviousTransform>(entity);
            world->emplace<MeshRenderer>(entity, MeshRenderer{
                MeshHandle{m_cube_mesh.id}, MaterialHandle{mat.id}, 0, true, true, true
            });
        }
    }

    // ---- Glass Sphere (transparency test) ----
    void create_glass_sphere(World* world, render::IRenderer* renderer) {
        render::MaterialData mat_data;
        mat_data.albedo = Vec4{0.6f, 0.8f, 1.0f, 0.35f};
        mat_data.roughness = 0.1f;
        mat_data.metallic = 0.0f;
        mat_data.transparent = true;
        mat_data.alpha_cutoff = 0.0f; // Disable alpha test to prevent discard
        auto mat = renderer->create_material(mat_data);
        m_materials.push_back(mat);

        auto entity = world->create("GlassSphere");
        auto& tf = world->emplace<LocalTransform>(entity, Vec3{2.0f, 1.2f, 3.5f});
        tf.scale = Vec3{1.4f};
        world->emplace<WorldTransform>(entity);
        world->emplace<PreviousTransform>(entity);
        world->emplace<MeshRenderer>(entity, MeshRenderer{
            MeshHandle{m_sphere_mesh.id}, MaterialHandle{mat.id}, 0, true, false, true, 2
        });
    }

    render::MeshHandle m_sphere_mesh;
    render::MeshHandle m_cube_mesh;
    render::MeshHandle m_plane_mesh;
    std::vector<render::MaterialHandle> m_materials;
};

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int) {
    // Parse command line for --screenshot support
    int argc = __argc;
    char** argv = __argv;
    RenderTestApp app;
    return app.run(argc, argv);
}
#else
int main(int argc, char** argv) {
    RenderTestApp app;
    return app.run(argc, argv);
}
#endif
