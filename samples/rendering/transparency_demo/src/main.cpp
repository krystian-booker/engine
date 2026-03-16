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

class TransparencyDemoApp : public Application {
protected:
    void on_init() override {
        log(LogLevel::Info, "[TransparencyDemo] Initializing transparency demo scene...");

        auto* renderer = get_renderer();
        if (!renderer) {
            log(LogLevel::Error, "[TransparencyDemo] Renderer not available");
            quit();
            return;
        }

        auto* world = get_world();
        if (!world) {
            log(LogLevel::Error, "[TransparencyDemo] World not available");
            quit();
            return;
        }

        auto* pipeline = get_render_pipeline();
        if (pipeline) {
            render::RenderPipelineConfig config;
            config.quality = render::RenderQuality::High;
            config.clear_color = 0x203040FF;
            config.enabled_passes = render::RenderPassFlags::DepthPrepass | 
                                    render::RenderPassFlags::GBuffer | 
                                    render::RenderPassFlags::MainOpaque | 
                                    render::RenderPassFlags::Transparent |
                                    render::RenderPassFlags::PostProcess | 
                                    render::RenderPassFlags::Final;
            pipeline->set_config(config);
        }

        // --- Primitives ---
        m_sphere_mesh = renderer->create_primitive(render::PrimitiveMesh::Sphere, 1.0f);
        m_cube_mesh = renderer->create_primitive(render::PrimitiveMesh::Cube, 1.0f);
        m_plane_mesh = renderer->create_primitive(render::PrimitiveMesh::Quad, 1.0f);

        // --- Background Objects (Opaque) ---
        // Red background cube
        render::MaterialData red_mat;
        red_mat.albedo = Vec4(1.0f, 0.1f, 0.1f, 1.0f);
        red_mat.roughness = 0.5f;
        red_mat.metallic = 0.1f;
        auto red_material = renderer->create_material(red_mat);

        // Green background sphere
        render::MaterialData green_mat;
        green_mat.albedo = Vec4(0.1f, 1.0f, 0.1f, 1.0f);
        green_mat.roughness = 0.5f;
        green_mat.metallic = 0.1f;
        auto green_material = renderer->create_material(green_mat);

        // Blue background cube
        render::MaterialData blue_mat;
        blue_mat.albedo = Vec4(0.1f, 0.1f, 1.0f, 1.0f);
        blue_mat.roughness = 0.5f;
        blue_mat.metallic = 0.1f;
        auto blue_material = renderer->create_material(blue_mat);

        // --- Transparent Objects ---
        // 1. Glass Window Pane (Thin Cube)
        render::MaterialData glass_pane_mat;
        glass_pane_mat.blend_mode = render::MaterialBlendMode::Transmission;
        glass_pane_mat.transmission = 1.0f;
        glass_pane_mat.ior = 1.52f; // Crown glass
        glass_pane_mat.thickness = 0.02f;
        glass_pane_mat.roughness = 0.02f;
        glass_pane_mat.metallic = 0.0f;
        glass_pane_mat.double_sided = true;
        glass_pane_mat.albedo = Vec4(0.95f, 0.98f, 1.0f, 1.0f);
        auto glass_pane_material = renderer->create_material(glass_pane_mat);

        // 2. Glass Ball
        render::MaterialData glass_ball_mat;
        glass_ball_mat.blend_mode = render::MaterialBlendMode::Transmission;
        glass_ball_mat.transmission = 1.0f;
        glass_ball_mat.ior = 1.52f;
        glass_ball_mat.thickness = 0.06f;
        glass_ball_mat.roughness = 0.03f;
        glass_ball_mat.metallic = 0.0f;
        glass_ball_mat.albedo = Vec4(0.92f, 0.97f, 1.0f, 1.0f);
        auto glass_ball_material = renderer->create_material(glass_ball_mat);

        // 3. Frosted Glass Ball
        render::MaterialData frosted_ball_mat;
        frosted_ball_mat.blend_mode = render::MaterialBlendMode::Transmission;
        frosted_ball_mat.transmission = 0.95f;
        frosted_ball_mat.ior = 1.52f;
        frosted_ball_mat.thickness = 0.05f;
        frosted_ball_mat.roughness = 0.45f;
        frosted_ball_mat.metallic = 0.0f;
        frosted_ball_mat.albedo = Vec4(0.95f, 0.98f, 1.0f, 1.0f);
        auto frosted_ball_material = renderer->create_material(frosted_ball_mat);

        // 4. Tinted Glass Pane
        render::MaterialData tinted_glass_mat;
        tinted_glass_mat.blend_mode = render::MaterialBlendMode::Transmission;
        tinted_glass_mat.transmission = 1.0f;
        tinted_glass_mat.ior = 1.52f;
        tinted_glass_mat.thickness = 0.02f;
        tinted_glass_mat.roughness = 0.1f;
        tinted_glass_mat.metallic = 0.0f;
        tinted_glass_mat.double_sided = true;
        tinted_glass_mat.albedo = Vec4(1.0f, 0.78f, 0.28f, 1.0f);
        auto tinted_glass_material = renderer->create_material(tinted_glass_mat);

        m_materials.push_back(red_material);
        m_materials.push_back(green_material);
        m_materials.push_back(blue_material);
        m_materials.push_back(glass_pane_material);
        m_materials.push_back(glass_ball_material);
        m_materials.push_back(frosted_ball_material);
        m_materials.push_back(tinted_glass_material);

        // --- Create Entities ---

        // Background objects (Z = -2.0f)
        auto ent_red = world->create("RedCube");
        world->emplace<LocalTransform>(ent_red, Vec3(-2.0f, 0.0f, -2.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), Vec3(2.0f, 2.0f, 0.5f));
        world->emplace<WorldTransform>(ent_red);
        world->emplace<PreviousTransform>(ent_red);
        world->emplace<MeshRenderer>(ent_red, MeshRenderer{engine::scene::MeshHandle{m_cube_mesh.id}, engine::scene::MaterialHandle{red_material.id}, 0, true, true, true, 0});

        auto ent_green = world->create("GreenSphere");
        world->emplace<LocalTransform>(ent_green, Vec3(0.0f, 0.0f, -2.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), Vec3(1.5f, 1.5f, 1.5f));
        world->emplace<WorldTransform>(ent_green);
        world->emplace<PreviousTransform>(ent_green);
        world->emplace<MeshRenderer>(ent_green, MeshRenderer{engine::scene::MeshHandle{m_sphere_mesh.id}, engine::scene::MaterialHandle{green_material.id}, 0, true, true, true, 0});

        auto ent_blue = world->create("BlueCube");
        world->emplace<LocalTransform>(ent_blue, Vec3(2.0f, 0.0f, -2.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), Vec3(2.0f, 2.0f, 0.5f));
        world->emplace<WorldTransform>(ent_blue);
        world->emplace<PreviousTransform>(ent_blue);
        world->emplace<MeshRenderer>(ent_blue, MeshRenderer{engine::scene::MeshHandle{m_cube_mesh.id}, engine::scene::MaterialHandle{blue_material.id}, 0, true, true, true, 0});

        // Foreground Transparent Objects (Z = 0.0f)
        
        // Glass Pane (left) - rotated to catch light
        auto ent_pane1 = world->create("GlassPane");
        world->emplace<LocalTransform>(ent_pane1, Vec3(-2.0f, 0.0f, 0.0f), glm::angleAxis(glm::radians(30.0f), Vec3(0.0f, 1.0f, 0.0f)), Vec3(1.5f, 1.5f, 0.1f));
        world->emplace<WorldTransform>(ent_pane1);
        world->emplace<PreviousTransform>(ent_pane1);
        world->emplace<MeshRenderer>(ent_pane1, MeshRenderer{engine::scene::MeshHandle{m_cube_mesh.id}, engine::scene::MaterialHandle{glass_pane_material.id}, 0, true, true, true, 0});

        // Glass Ball (center)
        auto ent_ball1 = world->create("GlassBall");
        world->emplace<LocalTransform>(ent_ball1, Vec3(-0.5f, 0.0f, 0.0f));
        world->emplace<WorldTransform>(ent_ball1);
        world->emplace<PreviousTransform>(ent_ball1);
        world->emplace<MeshRenderer>(ent_ball1, MeshRenderer{engine::scene::MeshHandle{m_sphere_mesh.id}, engine::scene::MaterialHandle{glass_ball_material.id}, 0, true, true, true, 0});

        // Frosted Ball (center-right)
        auto ent_ball2 = world->create("FrostedBall");
        world->emplace<LocalTransform>(ent_ball2, Vec3(1.0f, 0.0f, 0.0f));
        world->emplace<WorldTransform>(ent_ball2);
        world->emplace<PreviousTransform>(ent_ball2);
        world->emplace<MeshRenderer>(ent_ball2, MeshRenderer{engine::scene::MeshHandle{m_sphere_mesh.id}, engine::scene::MaterialHandle{frosted_ball_material.id}, 0, true, true, true, 0});

        // Tinted Glass Pane (right) - rotated
        auto ent_pane2 = world->create("TintedPane");
        world->emplace<LocalTransform>(ent_pane2, Vec3(2.5f, 0.0f, 0.0f), glm::angleAxis(glm::radians(-30.0f), Vec3(0.0f, 1.0f, 0.0f)), Vec3(1.5f, 1.5f, 0.1f));
        world->emplace<WorldTransform>(ent_pane2);
        world->emplace<PreviousTransform>(ent_pane2);
        world->emplace<MeshRenderer>(ent_pane2, MeshRenderer{engine::scene::MeshHandle{m_cube_mesh.id}, engine::scene::MaterialHandle{tinted_glass_material.id}, 0, true, true, true, 0});


        // --- Camera ---
        auto camera_entity = world->create("Camera");
        world->emplace<LocalTransform>(camera_entity, Vec3(0.0f, 0.0f, 6.0f));
        world->emplace<WorldTransform>(camera_entity);
        
        Camera cam;
        cam.fov = 60.0f;
        cam.aspect_ratio = static_cast<float>(window_width()) / static_cast<float>(window_height());
        cam.near_plane = 0.1f;
        cam.far_plane = 100.0f;
        cam.active = true;
        world->emplace<Camera>(camera_entity, cam);

        // --- Light ---
        renderer->set_hemisphere_ambient(
            Vec3{1.0f, 1.0f, 1.0f}, 0.5f,
            Vec3{0.5f, 0.5f, 0.6f}
        );
        renderer->set_ibl_intensity(1.0f);

        auto light_entity = world->create("DirectionalLight");
        Vec3 dir = glm::normalize(Vec3(-1.0f, -2.0f, -1.0f));
        Vec3 up = (std::abs(dir.y) > 0.99f) ? Vec3(0.0f, 0.0f, 1.0f) : Vec3(0.0f, 1.0f, 0.0f);
        world->emplace<LocalTransform>(light_entity, Vec3(0.0f), glm::quatLookAt(dir, up));
        world->emplace<WorldTransform>(light_entity);

        Light light;
        light.type = LightType::Directional;
        light.color = Vec3(1.0f, 1.0f, 1.0f);
        light.intensity = 10.0f;
        light.cast_shadows = true;
        light.enabled = true;
        world->emplace<Light>(light_entity, light);
    }

private:
    render::MeshHandle m_sphere_mesh;
    render::MeshHandle m_cube_mesh;
    render::MeshHandle m_plane_mesh;
    std::vector<render::MaterialHandle> m_materials;
};

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int) {
    int argc = __argc;
    char** argv = __argv;
    TransparencyDemoApp app;
    return app.run(argc, argv);
}
#else
int main(int argc, char** argv) {
    TransparencyDemoApp app;
    return app.run(argc, argv);
}
#endif
