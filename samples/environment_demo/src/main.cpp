// Environment Demo
// Demonstrates TimeOfDay and SkyController systems

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
#include <engine/environment/environment.hpp>

using namespace engine::core;
using namespace engine::scene;
namespace render = engine::render;
namespace env = engine::environment;

class EnvironmentDemo : public Application {
public:
    EnvironmentDemo() = default;

protected:
    void on_init() override {
        log(LogLevel::Info, "Environment Demo starting...");

        // Get renderer from base Application class (already initialized)
        auto* renderer = get_renderer();
        if (!renderer) {
            log(LogLevel::Error, "Renderer not available");
            quit();
            return;
        }

        // Create meshes for the scene
        m_ground_mesh = renderer->create_primitive(render::PrimitiveMesh::Plane, 20.0f);
        m_cube_mesh = renderer->create_primitive(render::PrimitiveMesh::Cube, 1.0f);
        m_sphere_mesh = renderer->create_primitive(render::PrimitiveMesh::Sphere, 0.5f);

        // Create the ECS world and scheduler
        m_world = std::make_unique<World>();
        m_scheduler = std::make_unique<Scheduler>();

        // Register transform system
        m_scheduler->add(Phase::FixedUpdate, transform_system, "transform", 0);

        // Create ground plane
        Entity ground = m_world->create("Ground");
        m_world->emplace<LocalTransform>(ground, Vec3{0.0f, 0.0f, 0.0f});
        m_world->emplace<WorldTransform>(ground);
        m_world->emplace<PreviousTransform>(ground);
        m_world->emplace<MeshRenderer>(ground, MeshRenderer{
            MeshHandle{m_ground_mesh.id},
            MaterialHandle{},
            0, true, false, true
        });

        // Create some objects to show lighting
        Entity cube = m_world->create("Cube");
        m_world->emplace<LocalTransform>(cube, Vec3{-2.0f, 0.5f, 0.0f});
        m_world->emplace<WorldTransform>(cube);
        m_world->emplace<PreviousTransform>(cube);
        m_world->emplace<MeshRenderer>(cube, MeshRenderer{
            MeshHandle{m_cube_mesh.id},
            MaterialHandle{},
            0, true, true, true
        });

        Entity sphere = m_world->create("Sphere");
        m_world->emplace<LocalTransform>(sphere, Vec3{2.0f, 0.5f, 0.0f});
        m_world->emplace<WorldTransform>(sphere);
        m_world->emplace<PreviousTransform>(sphere);
        m_world->emplace<MeshRenderer>(sphere, MeshRenderer{
            MeshHandle{m_sphere_mesh.id},
            MaterialHandle{},
            0, true, true, true
        });

        // Create camera
        Entity camera = m_world->create("MainCamera");
        m_world->emplace<LocalTransform>(camera, Vec3{0.0f, 3.0f, 10.0f});
        m_world->emplace<WorldTransform>(camera);
        m_world->emplace<Camera>(camera);

        // Initialize TimeOfDay system
        env::TimeOfDayConfig tod_config;
        tod_config.day_length_minutes = 2.0f;  // 2 real minutes = 24 game hours (fast demo)
        tod_config.start_hour = 6.0f;           // Start at dawn
        tod_config.latitude = 45.0f;
        env::get_time_of_day().initialize(tod_config);

        // Initialize SkyController
        env::get_sky_controller().initialize();

        // Register period change callback for logging
        env::get_time_of_day().on_period_change([](env::TimePeriod old_p, env::TimePeriod new_p) {
            log(LogLevel::Info, "[Environment] Period changed: {} -> {}",
                env::time_period_to_string(old_p),
                env::time_period_to_string(new_p));
        });

        log(LogLevel::Info, "Environment Demo initialized");
        log(LogLevel::Info, "Controls:");
        log(LogLevel::Info, "  Space: Pause/Resume time");
        log(LogLevel::Info, "  +/-: Speed up/slow down time");
        log(LogLevel::Info, "  1-8: Jump to period (Dawn, Morning, Noon, etc.)");
    }

    void on_shutdown() override {
        log(LogLevel::Info, "Environment Demo shutting down...");

        // Shutdown environment systems
        env::get_time_of_day().shutdown();
        env::get_sky_controller().shutdown();

        m_world.reset();
        m_scheduler.reset();

        // Destroy our meshes (renderer shutdown handled by base Application class)
        if (auto* renderer = get_renderer()) {
            renderer->destroy_mesh(m_ground_mesh);
            renderer->destroy_mesh(m_cube_mesh);
            renderer->destroy_mesh(m_sphere_mesh);
        }
    }

    void on_fixed_update(double dt) override {
        // Handle keyboard input for time controls
        handle_input();

        // Update environment systems
        env::get_time_of_day().update(dt);
        env::get_sky_controller().update(dt);

        // Log current time periodically
        m_log_timer += dt;
        if (m_log_timer >= 2.0) {  // Log every 2 seconds
            m_log_timer = 0.0;
            auto& tod = env::get_time_of_day();
            log(LogLevel::Info, "[Time] {:02d}:{:02d} - {} (scale: {:.1f}x)",
                static_cast<int>(tod.get_time()),
                static_cast<int>((tod.get_time() - static_cast<int>(tod.get_time())) * 60),
                env::time_period_to_string(tod.get_current_period()),
                tod.get_time_scale());
        }

        // Run ECS systems
        if (m_scheduler && m_world) {
            m_scheduler->run(*m_world, dt, Phase::FixedUpdate);
        }
    }

    void on_update(double /*dt*/) override {
        // Variable update - nothing for now
    }

    void on_render(double /*alpha*/) override {
        auto* renderer = get_renderer();
        if (!renderer || !m_world) return;

        // Get sky colors from SkyController for clear color
        auto& sky = env::get_sky_controller();
        auto gradient = sky.get_current_gradient();
        uint32_t clear_color = vec3_to_rgba(gradient.horizon_color);

        // Reset view 0 to render to backbuffer (not shadow map)
        // The shadow system configures view 0 for shadow cascades during init,
        // so we need to reconfigure it for backbuffer rendering each frame.
        render::ViewConfig view_config;
        view_config.render_target = render::RenderTargetHandle{};  // Invalid = backbuffer
        view_config.clear_color_enabled = true;
        view_config.clear_color = clear_color;
        view_config.clear_depth_enabled = true;
        view_config.clear_depth = 1.0f;
        view_config.viewport_width = static_cast<uint16_t>(window_width());
        view_config.viewport_height = static_cast<uint16_t>(window_height());
        renderer->configure_view(static_cast<render::RenderView>(0), view_config);

        renderer->begin_frame();

        // Clear screen (already set via ViewConfig above, but keep for consistency)
        renderer->clear(clear_color, 1.0f);

        // Set up camera looking at scene
        float aspect = static_cast<float>(window_width()) / static_cast<float>(window_height());
        Mat4 view = glm::lookAt(
            Vec3{0.0f, 5.0f, 15.0f},   // Camera position
            Vec3{0.0f, 0.0f, 0.0f},    // Look at origin
            Vec3{0.0f, 1.0f, 0.0f}     // Up vector
        );
        Mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 100.0f);
        renderer->set_camera(view, proj);

        // Get sun direction and intensity for lighting
        auto& tod = env::get_time_of_day();
        Vec3 sun_dir = tod.get_sun_direction();
        float sun_intensity = tod.get_sun_intensity();

        // Set directional light (sun)
        // Note: In a full implementation, this would set light uniforms
        (void)sun_dir;
        (void)sun_intensity;

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
    }

private:
    void handle_input() {
        auto& tod = env::get_time_of_day();

        // Check for key presses (simplified - in a real app, use proper input system)
        #ifdef _WIN32
        // Space - pause/resume
        if (GetAsyncKeyState(VK_SPACE) & 0x0001) {
            if (tod.is_paused()) {
                tod.resume();
                log(LogLevel::Info, "[Time] Resumed");
            } else {
                tod.pause();
                log(LogLevel::Info, "[Time] Paused");
            }
        }

        // + key - speed up
        if ((GetAsyncKeyState(VK_OEM_PLUS) & 0x0001) || (GetAsyncKeyState(VK_ADD) & 0x0001)) {
            float scale = tod.get_time_scale();
            tod.set_time_scale(std::min(scale * 2.0f, 32.0f));
            log(LogLevel::Info, "[Time] Speed: {:.1f}x", tod.get_time_scale());
        }

        // - key - slow down
        if ((GetAsyncKeyState(VK_OEM_MINUS) & 0x0001) || (GetAsyncKeyState(VK_SUBTRACT) & 0x0001)) {
            float scale = tod.get_time_scale();
            tod.set_time_scale(std::max(scale * 0.5f, 0.125f));
            log(LogLevel::Info, "[Time] Speed: {:.1f}x", tod.get_time_scale());
        }

        // Number keys 1-8 for time periods
        if (GetAsyncKeyState('1') & 0x0001) { tod.set_time(6.0f);  log(LogLevel::Info, "[Time] Set to Dawn"); }
        if (GetAsyncKeyState('2') & 0x0001) { tod.set_time(9.0f);  log(LogLevel::Info, "[Time] Set to Morning"); }
        if (GetAsyncKeyState('3') & 0x0001) { tod.set_time(12.0f); log(LogLevel::Info, "[Time] Set to Noon"); }
        if (GetAsyncKeyState('4') & 0x0001) { tod.set_time(15.0f); log(LogLevel::Info, "[Time] Set to Afternoon"); }
        if (GetAsyncKeyState('5') & 0x0001) { tod.set_time(18.0f); log(LogLevel::Info, "[Time] Set to Dusk"); }
        if (GetAsyncKeyState('6') & 0x0001) { tod.set_time(20.0f); log(LogLevel::Info, "[Time] Set to Evening"); }
        if (GetAsyncKeyState('7') & 0x0001) { tod.set_time(23.0f); log(LogLevel::Info, "[Time] Set to Night"); }
        if (GetAsyncKeyState('8') & 0x0001) { tod.set_time(3.0f);  log(LogLevel::Info, "[Time] Set to Midnight"); }
        #endif
    }

    static uint32_t vec3_to_rgba(const Vec3& color) {
        uint8_t r = static_cast<uint8_t>(std::clamp(color.r, 0.0f, 1.0f) * 255.0f);
        uint8_t g = static_cast<uint8_t>(std::clamp(color.g, 0.0f, 1.0f) * 255.0f);
        uint8_t b = static_cast<uint8_t>(std::clamp(color.b, 0.0f, 1.0f) * 255.0f);
        return (r << 24) | (g << 16) | (b << 8) | 0xFF;
    }

    std::unique_ptr<World> m_world;
    std::unique_ptr<Scheduler> m_scheduler;

    render::MeshHandle m_ground_mesh;
    render::MeshHandle m_cube_mesh;
    render::MeshHandle m_sphere_mesh;

    double m_log_timer = 0.0;
};

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    EnvironmentDemo app;
    return app.run();
}
#else
int main(int argc, char** argv) {
    EnvironmentDemo app;
    return app.run(argc, argv);
}
#endif
