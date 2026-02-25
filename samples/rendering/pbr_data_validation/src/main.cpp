#include <engine/core/application.hpp>
#include <engine/core/log.hpp>
#include <engine/core/math.hpp>
#include <engine/scene/scene.hpp>
#include <engine/scene/transform.hpp>
#include <engine/render/renderer.hpp>
#include <engine/render/render_pipeline.hpp> // Added for RenderPipelineConfig
#include <engine/core/filesystem.hpp>
#include <bgfx/bgfx.h>

using namespace engine::core;
using namespace engine::scene;
using namespace engine::render;

// =============================================================================
// PBR Data Validation Sample
// =============================================================================

class PbrDataValidationSample : public Application {
protected:
    void on_init() override {
        log(LogLevel::Info, "PBR Data Validation Initializing...");
        
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

        // Load debug shader
        std::string shader_path = renderer->get_shader_path();
        ShaderData shader_data;
        shader_data.vertex_binary = FileSystem::read_binary(shader_path + "vs_pbr_data_validation.sc.bin");
        shader_data.fragment_binary = FileSystem::read_binary(shader_path + "fs_pbr_data_validation.sc.bin");
        m_shader = renderer->create_shader(shader_data);
        
        if (!m_shader.valid()) {
            log(LogLevel::Error, "Failed to load validation shaders!");
            return;
        }

        m_sphere_mesh = renderer->create_primitive(PrimitiveMesh::Sphere, 2.0f);
        
        MaterialData mat;
        mat.shader = m_shader;
        mat.albedo = Vec4(1.0f);
        m_material = renderer->create_material(mat);
        
        // Spawn our test sphere
        m_sphere_ent = world->create("TestSphere");
        world->registry().emplace<LocalTransform>(m_sphere_ent, Vec3(0.0f, 0.0f, 0.0f));
        world->registry().emplace<WorldTransform>(m_sphere_ent);
        world->registry().emplace<PreviousTransform>(m_sphere_ent);
        world->registry().emplace<MeshRenderer>(m_sphere_ent, MeshRenderer{
            engine::scene::MeshHandle{m_sphere_mesh.id},
            engine::scene::MaterialHandle{m_material.id},
            0, true, false, false
        });

        // Set up static camera
        m_camera = world->create("Camera");
        world->registry().emplace<LocalTransform>(m_camera, Vec3(0.0f, 0.0f, 6.0f),
            glm::quatLookAt(glm::normalize(Vec3(0.0f, 0.0f, 0.0f) - Vec3(0.0f, 0.0f, 6.0f)), Vec3(0.0f, 1.0f, 0.0f)));
        world->registry().emplace<WorldTransform>(m_camera);
        
        Camera cam_comp;
        cam_comp.fov = 60.0f;
        cam_comp.aspect_ratio = static_cast<float>(window_width()) / static_cast<float>(window_height());
        cam_comp.near_plane = 0.1f;
        cam_comp.far_plane = 100.0f;
        cam_comp.active = true;
        world->registry().emplace<Camera>(m_camera, cam_comp);

        // Create explicit uniforms
        m_u_cameraPos = bgfx::createUniform("u_cameraPos", bgfx::UniformType::Vec4);
        m_u_lightDir  = bgfx::createUniform("u_lightDir",  bgfx::UniformType::Vec4);
        m_u_debugMode = bgfx::createUniform("u_debugMode", bgfx::UniformType::Vec4);
    }
    
    void on_update(double dt) override {
        m_time += static_cast<float>(dt);
        
        auto* active_world = get_world();
        if (active_world) {
            engine::scene::transform_system(*active_world, dt);
        }

        // Animated light direction to see the effect clearly
        // Rotates around the Y axis
        Vec4 lightDir(-sin(m_time), -0.5f, -cos(m_time), 0.0f);
        Vec4 cameraPos(0.0f, 0.0f, 6.0f, 1.0f); // Match camera setup
        
        // Determine mode based on time: Cycle 0, 1, 2 every 3 seconds
        int mode = (static_cast<int>(m_time) / 3) % 3;
        Vec4 debugMode((float)mode, 0.0f, 0.0f, 0.0f);

        // EXPLICITLY BIND UNIFORMS EACH FRAME
        bgfx::setUniform(m_u_cameraPos, &cameraPos);
        bgfx::setUniform(m_u_lightDir,  &lightDir);
        bgfx::setUniform(m_u_debugMode, &debugMode);
        
        int current_sec = static_cast<int>(m_time);
        if (current_sec != m_last_sec && current_sec % 3 == 0) {
            log(LogLevel::Info, "[PBR Data Validation] Switching to Mode %d", mode);
            m_last_sec = current_sec;
        }
    }
    
    void on_shutdown() override {
        bgfx::destroy(m_u_cameraPos);
        bgfx::destroy(m_u_lightDir);
        bgfx::destroy(m_u_debugMode);

        auto* renderer = get_renderer();
        if (!renderer) return;
        if (m_material.valid()) renderer->destroy_material(m_material);
        if (m_sphere_mesh.valid()) renderer->destroy_mesh(m_sphere_mesh);
    }
    
private:
    float m_time = 0.0f;
    int m_last_sec = -1;

    engine::render::MeshHandle m_sphere_mesh;
    engine::render::ShaderHandle m_shader;
    engine::render::MaterialHandle m_material;
    
    Entity m_sphere_ent = NullEntity;
    Entity m_camera = NullEntity;
    
    bgfx::UniformHandle m_u_cameraPos;
    bgfx::UniformHandle m_u_lightDir;
    bgfx::UniformHandle m_u_debugMode;
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
