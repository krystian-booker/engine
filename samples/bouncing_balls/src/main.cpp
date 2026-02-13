// Bouncing Balls Demo
// 50 physics-enabled spheres bouncing inside a large box

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
#include <engine/core/project_settings.hpp>
#include <engine/scene/scene.hpp>
#include <engine/render/renderer.hpp>
#include <engine/render/types.hpp>
#include <engine/physics/physics_world.hpp>
#include <engine/physics/rigid_body_component.hpp>
#include <engine/core/input.hpp>

#include <random>
#include <vector>

using namespace engine::core;
using namespace engine::scene;
namespace render = engine::render;
namespace physics = engine::physics;

// Number of balls to spawn
constexpr int NUM_BALLS = 50;

// Box dimensions (half-extents for the container)
constexpr float BOX_HALF_SIZE = 10.0f;
constexpr float WALL_THICKNESS = 0.5f;

// Ball properties
constexpr float BALL_RADIUS = 0.5f;
constexpr float BALL_MASS = 1.0f;
constexpr float BALL_RESTITUTION = 0.8f;
constexpr float BALL_FRICTION = 0.3f;

class BouncingBallsApp : public Application {
public:
    BouncingBallsApp() = default;

protected:
    void on_init() override {
        log(LogLevel::Info, "[BouncingBalls] Starting...");

        auto* renderer = get_renderer();
        if (!renderer) {
            log(LogLevel::Error, "[BouncingBalls] Renderer not available");
            quit();
            return;
        }

        // Create meshes
        m_sphere_mesh = renderer->create_primitive(render::PrimitiveMesh::Sphere, BALL_RADIUS);
        m_cube_mesh = renderer->create_primitive(render::PrimitiveMesh::Cube, 1.0f);

        // Create ECS world and scheduler
        m_scene_world = std::make_unique<World>();
        m_scheduler = std::make_unique<Scheduler>();

        // Create physics world
        m_physics_world = std::make_unique<physics::PhysicsWorld>();
        PhysicsSettings physics_settings;
        physics_settings.gravity = Vec3{0.0f, -9.81f, 0.0f};
        m_physics_world->init(physics_settings);

        // Register transform system
        m_scheduler->add(Phase::FixedUpdate, transform_system, "transform", 0);

        // Create the box container (6 walls)
        create_walls();

        // Create bouncing balls
        create_balls();

        // Create camera
        Entity camera = m_scene_world->create("MainCamera");
        m_scene_world->emplace<LocalTransform>(camera, Vec3{0.0f, 5.0f, 25.0f});
        m_scene_world->emplace<WorldTransform>(camera);
        m_scene_world->emplace<Camera>(camera);

        log(LogLevel::Info, "[BouncingBalls] Initialized with {} balls", NUM_BALLS);
    }

    void on_shutdown() override {
        log(LogLevel::Info, "[BouncingBalls] Shutting down...");

        m_scene_world.reset();
        m_scheduler.reset();

        if (m_physics_world) {
            m_physics_world->shutdown();
            m_physics_world.reset();
        }

        if (auto* renderer = get_renderer()) {
            renderer->destroy_mesh(m_sphere_mesh);
            renderer->destroy_mesh(m_cube_mesh);
            for (auto& mat : m_ball_materials) {
                renderer->destroy_material(mat);
            }
            m_ball_materials.clear();
        }
    }

    void on_fixed_update(double dt) override {
        if (!m_physics_world || !m_scene_world) return;

        // Step physics simulation
        m_physics_world->step(dt);

        // Sync rigid bodies to transforms
        physics::rigid_body_sync_system(*m_scene_world, *m_physics_world, static_cast<float>(dt));

        // Run transform system
        if (m_scheduler) {
            m_scheduler->run(*m_scene_world, dt, Phase::FixedUpdate);
        }
    }

    void on_update(double /*dt*/) override {
        // Reset simulation on Space press
        if (Input::key_pressed(Key::Space)) {
            reset_simulation();
        }
    }

    void on_render(double /*alpha*/) override {
        auto* renderer = get_renderer();
        if (!renderer || !m_scene_world) return;

        // Configure view
        render::ViewConfig view_config;
        view_config.render_target = render::RenderTargetHandle{};
        view_config.clear_color_enabled = true;
        view_config.clear_color = 0x87CEEBff;  // Sky blue background
        view_config.clear_depth_enabled = true;
        view_config.clear_depth = 1.0f;
        view_config.viewport_width = static_cast<uint16_t>(window_width());
        view_config.viewport_height = static_cast<uint16_t>(window_height());
        renderer->configure_view(static_cast<render::RenderView>(0), view_config);

        renderer->begin_frame();
        renderer->clear(0x87CEEBff, 1.0f);
        renderer->set_ibl_intensity(0.35f);

        // Lighting setup
        render::LightData sun_light;
        sun_light.type = 0; // Directional
        sun_light.direction = glm::normalize(Vec3{-0.3f, -1.0f, -0.2f});
        sun_light.color = Vec3{1.0f, 0.95f, 0.9f}; // Warm sunlight
        sun_light.intensity = 2.0f;
        sun_light.cast_shadows = true;
        renderer->set_light(0, sun_light);

        // Add a secondary fill light (blue-ish) from opposite direction
        render::LightData fill_light;
        fill_light.type = 0; // Directional
        fill_light.direction = glm::normalize(Vec3{0.3f, -0.5f, 0.5f});
        fill_light.color = Vec3{0.7f, 0.8f, 1.0f}; // Cool fill
        fill_light.intensity = 0.8f;
        fill_light.cast_shadows = false;
        renderer->set_light(1, fill_light);

        // Back light for rim highlights
        render::LightData back_light;
        back_light.type = 0;
        back_light.direction = glm::normalize(Vec3{0.0f, -0.3f, 0.8f});
        back_light.color = Vec3{1.0f, 0.95f, 0.85f};
        back_light.intensity = 0.4f;
        back_light.cast_shadows = false;
        renderer->set_light(2, back_light);

        // Set up camera
        float aspect = static_cast<float>(window_width()) / static_cast<float>(window_height());
        Mat4 view = glm::lookAt(
            Vec3{0.0f, 5.0f, 25.0f},   // Camera position
            Vec3{0.0f, 0.0f, 0.0f},    // Look at center
            Vec3{0.0f, 1.0f, 0.0f}     // Up vector
        );
        Mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 100.0f);
        renderer->set_camera(view, proj);

        // Render all mesh renderers
        auto mesh_view = m_scene_world->view<WorldTransform, MeshRenderer>();
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
    void create_walls() {
        // Wall definitions: name, position, scale (for visual), half-extents (for physics)
        struct WallDef {
            const char* name;
            Vec3 position;
            Vec3 scale;
            Vec3 half_extents;
        };

        WallDef walls[] = {
            // Floor
            {"Floor", {0.0f, -BOX_HALF_SIZE, 0.0f},
             {BOX_HALF_SIZE * 2.0f, WALL_THICKNESS * 2.0f, BOX_HALF_SIZE * 2.0f},
             {BOX_HALF_SIZE, WALL_THICKNESS, BOX_HALF_SIZE}},
            // Ceiling
            {"Ceiling", {0.0f, BOX_HALF_SIZE, 0.0f},
             {BOX_HALF_SIZE * 2.0f, WALL_THICKNESS * 2.0f, BOX_HALF_SIZE * 2.0f},
             {BOX_HALF_SIZE, WALL_THICKNESS, BOX_HALF_SIZE}},
            // Left wall
            {"LeftWall", {-BOX_HALF_SIZE, 0.0f, 0.0f},
             {WALL_THICKNESS * 2.0f, BOX_HALF_SIZE * 2.0f, BOX_HALF_SIZE * 2.0f},
             {WALL_THICKNESS, BOX_HALF_SIZE, BOX_HALF_SIZE}},
            // Right wall
            {"RightWall", {BOX_HALF_SIZE, 0.0f, 0.0f},
             {WALL_THICKNESS * 2.0f, BOX_HALF_SIZE * 2.0f, BOX_HALF_SIZE * 2.0f},
             {WALL_THICKNESS, BOX_HALF_SIZE, BOX_HALF_SIZE}},
            // Back wall
            {"BackWall", {0.0f, 0.0f, -BOX_HALF_SIZE},
             {BOX_HALF_SIZE * 2.0f, BOX_HALF_SIZE * 2.0f, WALL_THICKNESS * 2.0f},
             {BOX_HALF_SIZE, BOX_HALF_SIZE, WALL_THICKNESS}},
            // Front wall (transparent or not rendered so we can see inside)
            {"FrontWall", {0.0f, 0.0f, BOX_HALF_SIZE},
             {BOX_HALF_SIZE * 2.0f, BOX_HALF_SIZE * 2.0f, WALL_THICKNESS * 2.0f},
             {BOX_HALF_SIZE, BOX_HALF_SIZE, WALL_THICKNESS}},
        };

        // Floor material (Dark matte)
        render::MaterialData floor_mat_data;
        floor_mat_data.albedo = Vec4{0.9f, 0.9f, 0.92f, 1.0f};
        floor_mat_data.roughness = 0.9f;
        floor_mat_data.metallic = 0.0f;
        auto floor_mat = get_renderer()->create_material(floor_mat_data);
        m_ball_materials.push_back(floor_mat); // Track to destroy later

        // Wall material (Transparent glass-like)
        render::MaterialData wall_mat_data;
        wall_mat_data.albedo = Vec4{0.8f, 0.9f, 1.0f, 0.3f};
        wall_mat_data.roughness = 0.1f;
        wall_mat_data.metallic = 0.1f;
        wall_mat_data.transparent = true;
        auto wall_mat = get_renderer()->create_material(wall_mat_data);
        m_ball_materials.push_back(wall_mat);

        for (const auto& wall : walls) {
            Entity entity = m_scene_world->create(wall.name);

            // Transform
            m_scene_world->emplace<LocalTransform>(entity, wall.position);
            auto& local_tf = m_scene_world->get<LocalTransform>(entity);
            local_tf.scale = wall.scale;

            m_scene_world->emplace<WorldTransform>(entity);
            m_scene_world->emplace<PreviousTransform>(entity);

            // Mesh renderer
            bool is_floor = (wall.position.y < 0); /* Floor is distinct */
            m_scene_world->emplace<MeshRenderer>(entity, MeshRenderer{
                MeshHandle{m_cube_mesh.id},
                MaterialHandle{is_floor ? floor_mat.id : wall_mat.id},
                0,
                true,            // All visible now
                is_floor,        // only floor casts shadows to avoid visual clutter from walls
                true             // receive_shadows
            });

            // Physics - static box
            auto rb = physics::make_static_box(wall.half_extents);
            rb.friction = 0.5f;
            rb.restitution = 0.5f;  // Walls bouncy
            m_scene_world->emplace<physics::RigidBodyComponent>(entity, std::move(rb));
        }
    }

    void create_balls() {
        auto* renderer = get_renderer();

        // Random number generation
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> pos_dist(-BOX_HALF_SIZE + 2.0f, BOX_HALF_SIZE - 2.0f);
        std::uniform_real_distribution<float> height_dist(0.0f, BOX_HALF_SIZE - 2.0f);
        std::uniform_real_distribution<float> hue_dist(0.0f, 6.28318f); // 0 to 2*PI

        for (int i = 0; i < NUM_BALLS; ++i) {
            Entity ball = m_scene_world->create("Ball_" + std::to_string(i));

            // Random spawn position
            Vec3 position{
                pos_dist(gen),
                height_dist(gen),
                pos_dist(gen)
            };

            // Transform
            m_scene_world->emplace<LocalTransform>(ball, position);
            m_scene_world->emplace<WorldTransform>(ball);
            m_scene_world->emplace<PreviousTransform>(ball);

            // Create material with nicer colors (HSL-like logic)
            float hue = hue_dist(gen);
            // Simple cosine palette logic
            Vec3 color = Vec3{
                0.5f + 0.5f * std::cos(hue),
                0.5f + 0.5f * std::cos(hue + 2.094f), // + 2*PI/3
                0.5f + 0.5f * std::cos(hue + 4.188f)  // + 4*PI/3
            };
            
            render::MaterialData mat_data;
            mat_data.albedo = Vec4{color.x, color.y, color.z, 1.0f};
            mat_data.roughness = 0.3f;
            mat_data.metallic = 0.2f;
            auto mat_handle = renderer->create_material(mat_data);
            m_ball_materials.push_back(mat_handle);

            // Mesh renderer with colored material
            m_scene_world->emplace<MeshRenderer>(ball, MeshRenderer{
                MeshHandle{m_sphere_mesh.id},
                MaterialHandle{mat_handle.id},  // Convert render::MaterialHandle to scene::MaterialHandle
                0,
                true,   // visible
                true,   // cast_shadows
                true    // receive_shadows
            });

            // Physics - dynamic sphere
            auto rb = physics::make_dynamic_sphere(BALL_RADIUS, BALL_MASS);
            rb.restitution = BALL_RESTITUTION;
            rb.friction = BALL_FRICTION;
            m_scene_world->emplace<physics::RigidBodyComponent>(ball, std::move(rb));

            m_ball_entities.push_back(ball);
        }
    }

    std::unique_ptr<World> m_scene_world;
    std::unique_ptr<Scheduler> m_scheduler;
    std::unique_ptr<physics::PhysicsWorld> m_physics_world;

    render::MeshHandle m_sphere_mesh;
    render::MeshHandle m_cube_mesh;
    std::vector<render::MaterialHandle> m_ball_materials;

    std::vector<Entity> m_ball_entities;

    void reset_simulation() {
        log(LogLevel::Info, "[BouncingBalls] Resetting simulation...");
        auto* renderer = get_renderer();

        // 1. Destroy existing balls
        for (Entity e : m_ball_entities) {
            m_scene_world->destroy(e);
        }
        m_ball_entities.clear();

        // 2. Destroy ball materials (including walls/floor ones added to this list)
        for (auto& mat : m_ball_materials) {
            renderer->destroy_material(mat);
        }
        m_ball_materials.clear();

        // 3. Recreate clean state (walls then balls)
        // Resetting the world clears all entities, including camera and walls.
        // We need to fully rebuild the scene.
        m_scene_world->clear(); 

        // Re-create camera
        Entity camera = m_scene_world->create("MainCamera");
        m_scene_world->emplace<LocalTransform>(camera, Vec3{0.0f, 5.0f, 25.0f});
        m_scene_world->emplace<WorldTransform>(camera);
        m_scene_world->emplace<Camera>(camera);

        // Re-create walls (will create new materials)
        create_walls();

        // Re-create balls (will create new materials)
        create_balls();

        log(LogLevel::Info, "[BouncingBalls] Reset complete.");
    }
};

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    BouncingBallsApp app;
    return app.run();
}
#else
int main(int argc, char** argv) {
    BouncingBallsApp app;
    return app.run(argc, argv);
}
#endif
