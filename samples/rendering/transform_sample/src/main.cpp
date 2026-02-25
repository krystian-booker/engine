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
#include <engine/core/math.hpp>
#include <engine/scene/scene.hpp>
#include <engine/render/renderer.hpp>
#include <engine/render/render_pipeline.hpp>
#include <engine/core/filesystem.hpp>

using namespace engine::core;
using namespace engine::scene;
using namespace engine::render;

class TransformSample : public Application {
protected:
    void on_init() override {
        log(LogLevel::Info, "Transform Sample Initializing...");

        auto* renderer = get_renderer();
        auto* world = get_world();
        auto* pipeline = get_render_pipeline();

        if (!renderer || !world || !pipeline) return;

        // Configure pipeline
        RenderPipelineConfig config;
        config.quality = RenderQuality::High;
        config.enabled_passes = RenderPassFlags::DepthPrepass | RenderPassFlags::GBuffer | RenderPassFlags::MainOpaque | RenderPassFlags::PostProcess | RenderPassFlags::Final | RenderPassFlags::Debug;
        config.show_debug_overlay = false;
        config.debug_view_mode = DebugViewMode::None;
        pipeline->set_config(config);

        // Meshes
        m_sphere_mesh = renderer->create_primitive(PrimitiveMesh::Sphere, 1.0f);
        m_cube_mesh = renderer->create_primitive(PrimitiveMesh::Cube, 1.0f);

        // Materials
        MaterialData mat_data;
        mat_data.albedo = Vec4(0.8f, 0.8f, 0.8f, 1.0f);
        mat_data.roughness = 0.5f;
        mat_data.metallic = 0.0f;
        m_material = renderer->create_material(mat_data);

        m_broken_material = engine::render::MaterialHandle{999999};

        // Debug Materials
        std::string shader_path = renderer->get_shader_path();
        ShaderData debug_geom_shader_data;
        debug_geom_shader_data.vertex_binary = FileSystem::read_binary(shader_path + "vs_debug_geom.sc.bin");
        debug_geom_shader_data.fragment_binary = FileSystem::read_binary(shader_path + "fs_debug_geom.sc.bin");
        engine::render::ShaderHandle debug_geom_shader = renderer->create_shader(debug_geom_shader_data);

        MaterialData tangent_mat_data;
        tangent_mat_data.shader = debug_geom_shader;
        tangent_mat_data.albedo = Vec4(1.0f, 0.0f, 0.0f, 0.0f); // Mode 1 for Tangents
        m_tangent_material = renderer->create_material(tangent_mat_data);

        MaterialData bitangent_mat_data;
        bitangent_mat_data.shader = debug_geom_shader;
        bitangent_mat_data.albedo = Vec4(2.0f, 0.0f, 0.0f, 0.0f); // Mode 2 for Bitangents
        m_bitangent_material = renderer->create_material(bitangent_mat_data);

        // Background quad for linear space validation
        m_quad_mesh = renderer->create_primitive(PrimitiveMesh::Quad, 10.0f);
        MaterialData bg_mat_data;
        bg_mat_data.albedo = Vec4(0.5f, 0.5f, 0.5f, 1.0f); // 50% Grey validation
        bg_mat_data.roughness = 1.0f;
        bg_mat_data.metallic = 0.0f;
        m_bg_material = renderer->create_material(bg_mat_data);

        // Camera
        m_camera_entity = world->create("Camera");
        m_camera_pos = Vec3(0.0f, 2.0f, 5.0f);
        world->emplace<LocalTransform>(m_camera_entity, m_camera_pos);
        world->emplace<WorldTransform>(m_camera_entity);
        
        Camera cam;
        cam.fov = 60.0f;
        cam.aspect_ratio = static_cast<float>(window_width()) / static_cast<float>(window_height());
        cam.near_plane = 0.1f;
        cam.far_plane = 100.0f;
        cam.active = true;
        world->emplace<Camera>(m_camera_entity, cam);

        // Light
        auto light_entity = world->create("Light");
        Vec3 dir = glm::normalize(Vec3(-1.0f, -2.0f, -1.0f));
        Vec3 up = (std::abs(dir.y) > 0.99f) ? Vec3(0.0f, 0.0f, 1.0f) : Vec3(0.0f, 1.0f, 0.0f);
        world->emplace<LocalTransform>(light_entity, Vec3(0.0f), glm::quatLookAt(dir, up));
        world->emplace<WorldTransform>(light_entity);

        Light light;
        light.type = LightType::Directional;
        light.color = Vec3(1.0f);
        light.intensity = 5.0f;
        light.cast_shadows = false;
        light.enabled = true;
        world->emplace<Light>(light_entity, light);

        // Objects
        m_obj1 = world->create("Obj1");
        world->emplace<LocalTransform>(m_obj1, Vec3(-1.5f, 0.0f, 0.0f));
        world->emplace<WorldTransform>(m_obj1);
        world->emplace<PreviousTransform>(m_obj1);
        world->emplace<MeshRenderer>(m_obj1, MeshRenderer{engine::scene::MeshHandle{m_sphere_mesh.id}, engine::scene::MaterialHandle{m_material.id}, 0, true, true, true});

        m_obj2 = world->create("Obj2");
        // Non-uniform scale test
        world->emplace<LocalTransform>(m_obj2, Vec3(1.5f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), Vec3(1.0f, 2.0f, 1.0f));
        world->emplace<WorldTransform>(m_obj2);
        world->emplace<PreviousTransform>(m_obj2);
        world->emplace<MeshRenderer>(m_obj2, MeshRenderer{engine::scene::MeshHandle{m_cube_mesh.id}, engine::scene::MaterialHandle{m_material.id}, 0, true, true, true});

        m_bg_quad = world->create("BgQuad");
        // Rotate quad to face camera (-Z)
        world->emplace<LocalTransform>(m_bg_quad, Vec3(0.0f, 0.0f, -5.0f), glm::angleAxis(glm::radians(90.0f), Vec3(1.0f, 0.0f, 0.0f)));
        world->emplace<WorldTransform>(m_bg_quad);
        world->emplace<PreviousTransform>(m_bg_quad);
        world->emplace<MeshRenderer>(m_bg_quad, MeshRenderer{engine::scene::MeshHandle{m_quad_mesh.id}, engine::scene::MaterialHandle{m_bg_material.id}, 0, false, false, true});
    }

    void on_update(double dt) override {
        m_time += static_cast<float>(dt);

        auto* world = get_world();
        if (world) {
            auto& tf_cam = world->get<LocalTransform>(m_camera_entity);
            // Orbital camera movement
            float radius = 5.0f;
            float orbit_speed = 0.5f;
            float cam_x = std::sin(m_time * orbit_speed) * radius;
            float cam_z = std::cos(m_time * orbit_speed) * radius;
            tf_cam.position = Vec3(cam_x, 2.0f, cam_z);
            tf_cam.rotation = glm::quatLookAt(glm::normalize(Vec3(0.0f, 0.0f, 0.0f) - tf_cam.position), Vec3(0.0f, 1.0f, 0.0f));

            auto& tf1 = world->get<LocalTransform>(m_obj1);
            tf1.rotation = glm::angleAxis(m_time, Vec3(0.0f, 1.0f, 0.0f));

            auto& tf2 = world->get<LocalTransform>(m_obj2);
            tf2.rotation = glm::angleAxis(-m_time, glm::normalize(Vec3(1.0f, 1.0f, 0.0f)));

            int mode = static_cast<int>(m_time / 3.0f) % 6;

            auto* pipeline = get_render_pipeline();
            if (pipeline) {
                auto config = pipeline->get_config();
                auto& mr1 = world->get<MeshRenderer>(m_obj1);
                auto& mr2 = world->get<MeshRenderer>(m_obj2);

                // Reset to standard material unless specifically in a geometry test mode
                mr1.material = engine::scene::MaterialHandle{m_material.id};
                mr2.material = engine::scene::MaterialHandle{m_material.id};
                config.debug_view_mode = DebugViewMode::None;

                switch (mode) {
                    case 0: // Standard
                        break;
                    case 1: // Normals
                        config.debug_view_mode = DebugViewMode::Normals;
                        break;
                    case 2: // Tangents (Custom Material Swap)
                        mr1.material = engine::scene::MaterialHandle{m_tangent_material.id};
                        mr2.material = engine::scene::MaterialHandle{m_tangent_material.id};
                        break;
                    case 3: // Bitangents (Custom Material Swap)
                        mr1.material = engine::scene::MaterialHandle{m_bitangent_material.id};
                        mr2.material = engine::scene::MaterialHandle{m_bitangent_material.id};
                        break;
                    case 4: // Linear Depth
                        config.debug_view_mode = DebugViewMode::LinearDepth;
                        break;
                    case 5: // Error State
                        mr2.material = engine::scene::MaterialHandle{m_broken_material.id};
                        break;
                }
                pipeline->set_config(config);

                // Auto-capture screenshots for each mode exactly once, and quit after
                auto* renderer = get_renderer();
                if (renderer) {
                    if (m_time > 0.5f && !m_captured_standard) {
                        m_captured_standard = true;
                        renderer->save_screenshot("transform_demo_standard.png", pipeline->get_final_texture());
                    } else if (m_time > 3.5f && !m_captured_normals) {
                        m_captured_normals = true;
                        renderer->save_screenshot("transform_demo_normals.png", pipeline->get_final_texture());
                    } else if (m_time > 6.5f && !m_captured_tangents) {
                        m_captured_tangents = true;
                        renderer->save_screenshot("transform_demo_tangents.png", pipeline->get_final_texture());
                    } else if (m_time > 9.5f && !m_captured_bitangents) {
                        m_captured_bitangents = true;
                        renderer->save_screenshot("transform_demo_bitangents.png", pipeline->get_final_texture());
                    } else if (m_time > 12.5f && !m_captured_depth) {
                        m_captured_depth = true;
                        renderer->save_screenshot("transform_demo_depth.png", pipeline->get_final_texture());
                    } else if (m_time > 15.5f && !m_captured_pink) {
                        m_captured_pink = true;
                        renderer->save_screenshot("transform_demo_pink.png", pipeline->get_final_texture());
                    } else if (m_time > 20.0f) {
                        quit();
                    }
                }
            }
        }
    }

    void on_shutdown() override {
        if (auto* renderer = get_renderer()) {
            if (m_material.valid()) renderer->destroy_material(m_material);
            if (m_sphere_mesh.valid()) renderer->destroy_mesh(m_sphere_mesh);
            if (m_cube_mesh.valid()) renderer->destroy_mesh(m_cube_mesh);
        }
    }

private:
    float m_time = 0.0f;
    Vec3 m_camera_pos;
    engine::render::MeshHandle m_sphere_mesh;
    engine::render::MeshHandle m_cube_mesh;
    engine::render::MeshHandle m_quad_mesh;
    engine::render::MaterialHandle m_material;
    engine::render::MaterialHandle m_bg_material;
    engine::render::MaterialHandle m_broken_material;
    engine::render::MaterialHandle m_tangent_material;
    engine::render::MaterialHandle m_bitangent_material;
    
    Entity m_camera_entity = NullEntity;
    Entity m_obj1 = NullEntity;
    Entity m_obj2 = NullEntity;
    Entity m_bg_quad = NullEntity;

    bool m_captured_standard = false;
    bool m_captured_normals = false;
    bool m_captured_tangents = false;
    bool m_captured_bitangents = false;
    bool m_captured_depth = false;
    bool m_captured_pink = false;
};

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int) {
    int argc = __argc;
    char** argv = __argv;
    TransformSample app;
    return app.run(argc, argv);
}
#else
int main(int argc, char** argv) {
    TransformSample app;
    return app.run(argc, argv);
}
#endif
