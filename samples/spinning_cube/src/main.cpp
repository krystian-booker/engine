// Spinning Cube Demo

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
#include <engine/core/time.hpp>
#include <engine/scene/scene.hpp>
#include <engine/render/renderer.hpp>

using namespace engine::core;
using namespace engine::scene;
namespace render = engine::render;

class SpinningCubeApp : public Application {
public:
    SpinningCubeApp() = default;

protected:
    void on_init() override {
        log(LogLevel::Info, "Spinning Cube Demo starting...");

        // Get renderer from base Application class (already initialized)
        auto* renderer = get_renderer();
        if (!renderer) {
            log(LogLevel::Error, "Renderer not available");
            quit();
            return;
        }

        // Create a cube mesh
        m_cube_mesh = renderer->create_primitive(render::PrimitiveMesh::Cube, 1.0f);

        // Create the ECS world and scheduler
        m_world = std::make_unique<World>();
        m_scheduler = std::make_unique<Scheduler>();

        // Register transform system
        m_scheduler->add(Phase::FixedUpdate, transform_system, "transform", 0);

        // Create a cube entity
        m_cube_entity = m_world->create("SpinningCube");
        m_world->emplace<LocalTransform>(m_cube_entity, Vec3{0.0f, 0.0f, -5.0f});
        m_world->emplace<WorldTransform>(m_cube_entity);
        m_world->emplace<PreviousTransform>(m_cube_entity);
        m_world->emplace<MeshRenderer>(m_cube_entity, MeshRenderer{
            MeshHandle{m_cube_mesh.id},      // Convert render::MeshHandle to scene::MeshHandle
            MaterialHandle{},                 // MaterialHandle (none for now)
            0,                                // render_layer
            true,                             // visible
            true,                             // cast_shadows
            true                              // receive_shadows
        });

        // Create a camera entity
        Entity camera = m_world->create("MainCamera");
        m_world->emplace<LocalTransform>(camera, Vec3{0.0f, 0.0f, 0.0f});
        m_world->emplace<WorldTransform>(camera);
        m_world->emplace<Camera>(camera);

        log(LogLevel::Info, "Spinning Cube Demo initialized");
    }

    void on_shutdown() override {
        log(LogLevel::Info, "Spinning Cube Demo shutting down...");

        m_world.reset();
        m_scheduler.reset();

        // Destroy our mesh (renderer shutdown handled by base Application class)
        if (auto* renderer = get_renderer()) {
            renderer->destroy_mesh(m_cube_mesh);
        }
    }

    void on_fixed_update(double dt) override {
        // Rotate the cube
        if (m_world && m_world->valid(m_cube_entity)) {
            auto& transform = m_world->get<LocalTransform>(m_cube_entity);

            // Rotate around Y axis
            m_rotation_angle += static_cast<float>(dt) * 1.0f;  // 1 radian per second
            transform.rotation = glm::angleAxis(m_rotation_angle, Vec3{0.0f, 1.0f, 0.0f});

            // Also add some X rotation for visual interest
            transform.rotation = transform.rotation * glm::angleAxis(m_rotation_angle * 0.5f, Vec3{1.0f, 0.0f, 0.0f});
        }

        // Run ECS systems
        if (m_scheduler && m_world) {
            m_scheduler->run(*m_world, dt, Phase::FixedUpdate);
        }
    }

    void on_update(double dt) override {
        (void)dt;
        // Variable update - nothing for now
    }

    void on_render(double alpha) override {
        auto* renderer = get_renderer();
        if (!renderer || !m_world) return;

        // Reset view 0 to render to backbuffer (not shadow map)
        // The shadow system configures view 0 for shadow cascades during init,
        // so we need to reconfigure it for backbuffer rendering each frame.
        render::ViewConfig view_config;
        view_config.render_target = render::RenderTargetHandle{};  // Invalid = backbuffer
        view_config.clear_color_enabled = true;
        view_config.clear_color = 0x303030ff;
        view_config.clear_depth_enabled = true;
        view_config.clear_depth = 1.0f;
        view_config.viewport_width = static_cast<uint16_t>(window_width());
        view_config.viewport_height = static_cast<uint16_t>(window_height());
        renderer->configure_view(static_cast<render::RenderView>(0), view_config);

        renderer->begin_frame();

        // Clear screen (already set via ViewConfig above, but keep for consistency)
        renderer->clear(0x303030ff, 1.0f);

        // Set up camera
        float aspect = static_cast<float>(window_width()) / static_cast<float>(window_height());
        Mat4 view = glm::lookAt(
            Vec3{0.0f, 2.0f, 5.0f},   // Camera position
            Vec3{0.0f, 0.0f, 0.0f},   // Look at origin
            Vec3{0.0f, 1.0f, 0.0f}    // Up vector
        );
        Mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 100.0f);
        renderer->set_camera(view, proj);

        // Render all mesh renderers
        auto mesh_view = m_world->view<WorldTransform, MeshRenderer>();
        for (auto [entity, world_tf, mesh_renderer] : mesh_view.each()) {
            if (!mesh_renderer.visible) continue;

            render::DrawCall call;
            call.mesh = render::MeshHandle{mesh_renderer.mesh.id};
            call.material = render::MaterialHandle{mesh_renderer.material.id};
            call.transform = world_tf.matrix;
            call.render_layer = mesh_renderer.render_layer;
            call.cast_shadows = mesh_renderer.cast_shadows;

            renderer->queue_draw(call);
        }

        renderer->flush();
        renderer->end_frame();

        (void)alpha;  // Could use for interpolation
    }

private:
    std::unique_ptr<World> m_world;
    std::unique_ptr<Scheduler> m_scheduler;

    render::MeshHandle m_cube_mesh;
    Entity m_cube_entity = NullEntity;
    float m_rotation_angle = 0.0f;
};

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    SpinningCubeApp app;
    return app.run();
}
#else
int main(int argc, char** argv) {
    SpinningCubeApp app;
    return app.run(argc, argv);
}
#endif
