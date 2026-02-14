// Bouncing Balls Demo
// 50 physics-enabled spheres bouncing inside a large box
// Uses the engine's standard ECS rendering path (Camera, Light, MeshRenderer components)

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
#include <engine/physics/physics_system.hpp>
#include <engine/physics/rigid_body_component.hpp>
#include <engine/plugin/system_registry.hpp>
#include <engine/core/input.hpp>

#include <random>
#include <vector>

using namespace engine::core;
using namespace engine::scene;
namespace render = engine::render;
namespace physics = engine::physics;

// Number of balls to spawn
constexpr int NUM_BALLS = 100;

// Box dimensions (half-extents for the container)
constexpr float BOX_HALF_SIZE = 10.0f;
constexpr float WALL_THICKNESS = 0.5f;

// Ball properties
constexpr float BALL_RADIUS = 0.5f;
constexpr float BALL_MASS = 1.0f;
constexpr float BALL_RESTITUTION = 0.8f;
constexpr float BALL_FRICTION = 0.3f;
constexpr float BALL_LINEAR_DAMPING = 0.5f;
constexpr float BALL_ANGULAR_DAMPING = 0.2f;

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

        auto* world = get_world();
        if (!world) {
            log(LogLevel::Error, "[BouncingBalls] World not available");
            quit();
            return;
        }

        // Create meshes
        m_sphere_mesh = renderer->create_primitive(render::PrimitiveMesh::Sphere, BALL_RADIUS);
        m_cube_mesh = renderer->create_primitive(render::PrimitiveMesh::Cube, 1.0f);

        // Register physics systems with the engine's system registry
        auto* physics_system = get_physics_system();
        auto* sys_registry = get_system_registry();
        if (physics_system && sys_registry) {
            // Physics step runs before transform_fixed (priority 10)
            sys_registry->add(Phase::FixedUpdate, physics_system->create_step_system(), "physics_step", 12);
            // Rigid body sync runs after physics step, before transform
            sys_registry->add(Phase::FixedUpdate, physics_system->create_rigid_body_system(), "rigid_body_sync", 11);
        }

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
            config.clear_color = 0x4488CCFF;  // Sky blue background
            pipeline->set_config(config);
        }

        renderer->set_ibl_intensity(1.0f);

        // Create scene entities
        create_camera();
        create_lights();
        create_walls();
        create_balls();

        log(LogLevel::Info, "[BouncingBalls] Initialized with {} balls", NUM_BALLS);
    }

    void on_shutdown() override {
        log(LogLevel::Info, "[BouncingBalls] Shutting down...");

        // Remove registered physics systems
        auto* sys_registry = get_system_registry();
        if (sys_registry) {
            sys_registry->remove("physics_step");
            sys_registry->remove("rigid_body_sync");
        }

        if (auto* renderer = get_renderer()) {
            renderer->destroy_mesh(m_sphere_mesh);
            renderer->destroy_mesh(m_cube_mesh);
            for (auto& mat : m_materials) {
                renderer->destroy_material(mat);
            }
            m_materials.clear();
        }
    }

    void on_update(double /*dt*/) override {
        // Update camera aspect ratio from window dimensions
        auto* world = get_world();
        if (world && m_camera_entity != NullEntity) {
            auto* cam = world->try_get<Camera>(m_camera_entity);
            if (cam) {
                cam->aspect_ratio = static_cast<float>(window_width())
                                  / static_cast<float>(window_height());
            }
        }

        // Reset simulation on Space press
        if (Input::key_pressed(Key::Space)) {
            reset_simulation();
        }
    }

private:
    void create_camera() {
        auto* world = get_world();

        m_camera_entity = world->create("MainCamera");
        auto& local_tf = world->emplace<LocalTransform>(m_camera_entity, Vec3{0.0f, 5.0f, 25.0f});
        local_tf.look_at(Vec3{0.0f, 0.0f, 0.0f});
        world->emplace<WorldTransform>(m_camera_entity);

        float aspect = static_cast<float>(window_width())
                      / static_cast<float>(window_height());
        Camera cam;
        cam.fov = 60.0f;
        cam.aspect_ratio = aspect;
        cam.near_plane = 0.1f;
        cam.far_plane = 100.0f;
        cam.active = true;
        world->emplace<Camera>(m_camera_entity, cam);
    }

    void create_lights() {
        auto* world = get_world();

        // Light direction/color/intensity definitions
        struct LightDef {
            Vec3 direction;
            Vec3 color;
            float intensity;
            bool cast_shadows;
        };

        LightDef defs[] = {
            // Sun light
            {{-0.3f, -1.0f, -0.2f}, {1.0f, 0.95f, 0.9f}, 1.5f, true},
            // Fill light
            {{0.3f, -0.5f, 0.5f}, {0.7f, 0.8f, 1.0f}, 0.4f, false},
            // Back light
            {{0.0f, -0.3f, 0.8f}, {1.0f, 0.95f, 0.85f}, 0.2f, false},
        };

        for (const auto& def : defs) {
            Entity entity = world->create("Light");

            // Compute rotation from direction
            // light_gather_system extracts direction as: rot * Vec3(0, 0, -1)
            Vec3 dir = glm::normalize(def.direction);
            Vec3 up = (std::abs(dir.y) > 0.99f) ? Vec3(0.0f, 0.0f, 1.0f)
                                                  : Vec3(0.0f, 1.0f, 0.0f);
            Quat rot = glm::quatLookAt(dir, up);

            world->emplace<LocalTransform>(entity, Vec3{0.0f}, rot);
            world->emplace<WorldTransform>(entity);

            Light light;
            light.type = LightType::Directional;
            light.color = def.color;
            light.intensity = def.intensity;
            light.cast_shadows = def.cast_shadows;
            light.enabled = true;
            world->emplace<Light>(entity, light);

            m_light_entities.push_back(entity);
        }
    }

    void create_walls() {
        auto* world = get_world();

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
            // Front wall
            {"FrontWall", {0.0f, 0.0f, BOX_HALF_SIZE},
             {BOX_HALF_SIZE * 2.0f, BOX_HALF_SIZE * 2.0f, WALL_THICKNESS * 2.0f},
             {BOX_HALF_SIZE, BOX_HALF_SIZE, WALL_THICKNESS}},
        };

        // Floor material (light matte)
        render::MaterialData floor_mat_data;
        floor_mat_data.albedo = Vec4{0.9f, 0.9f, 0.92f, 1.0f};
        floor_mat_data.roughness = 0.9f;
        floor_mat_data.metallic = 0.0f;
        auto floor_mat = get_renderer()->create_material(floor_mat_data);
        m_materials.push_back(floor_mat);

        // Wall material (transparent glass-like)
        render::MaterialData wall_mat_data;
        wall_mat_data.albedo = Vec4{0.8f, 0.9f, 1.0f, 0.3f};
        wall_mat_data.roughness = 0.1f;
        wall_mat_data.metallic = 0.1f;
        wall_mat_data.transparent = true;
        auto wall_mat = get_renderer()->create_material(wall_mat_data);
        m_materials.push_back(wall_mat);

        for (const auto& wall : walls) {
            Entity entity = world->create(wall.name);

            // Transform
            auto& local_tf = world->emplace<LocalTransform>(entity, wall.position);
            local_tf.scale = wall.scale;
            world->emplace<WorldTransform>(entity);
            world->emplace<PreviousTransform>(entity);

            // Mesh renderer
            bool is_floor = (wall.position.y < 0);
            world->emplace<MeshRenderer>(entity, MeshRenderer{
                MeshHandle{m_cube_mesh.id},
                MaterialHandle{is_floor ? floor_mat.id : wall_mat.id},
                0,
                true,   // visible
                true,   // cast_shadows
                true    // receive_shadows
            });

            // Physics - static box
            auto rb = physics::make_static_box(wall.half_extents);
            rb.friction = 0.5f;
            rb.restitution = 0.5f;
            world->emplace<physics::RigidBodyComponent>(entity, std::move(rb));

            m_wall_entities.push_back(entity);
        }
    }

    void create_balls() {
        auto* world = get_world();
        auto* renderer = get_renderer();

        // Random number generation
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> pos_dist(-BOX_HALF_SIZE + 2.0f, BOX_HALF_SIZE - 2.0f);
        std::uniform_real_distribution<float> height_dist(0.0f, BOX_HALF_SIZE - 2.0f);
        std::uniform_real_distribution<float> hue_dist(0.0f, 6.28318f); // 0 to 2*PI

        for (int i = 0; i < NUM_BALLS; ++i) {
            Entity ball = world->create("Ball_" + std::to_string(i));

            // Random spawn position
            Vec3 position{
                pos_dist(gen),
                height_dist(gen),
                pos_dist(gen)
            };

            // Transform
            world->emplace<LocalTransform>(ball, position);
            world->emplace<WorldTransform>(ball);
            world->emplace<PreviousTransform>(ball);

            // Create material with colorful cosine palette
            float hue = hue_dist(gen);
            Vec3 color = Vec3{
                0.5f + 0.5f * std::cos(hue),
                0.5f + 0.5f * std::cos(hue + 2.094f),
                0.5f + 0.5f * std::cos(hue + 4.188f)
            };

            render::MaterialData mat_data;
            mat_data.albedo = Vec4{color.x, color.y, color.z, 1.0f};
            mat_data.roughness = 0.5f;
            mat_data.metallic = 0.2f;
            auto mat_handle = renderer->create_material(mat_data);
            m_materials.push_back(mat_handle);

            // Mesh renderer
            world->emplace<MeshRenderer>(ball, MeshRenderer{
                MeshHandle{m_sphere_mesh.id},
                MaterialHandle{mat_handle.id},
                0,
                true,   // visible
                true,   // cast_shadows
                true    // receive_shadows
            });

            // Physics - dynamic sphere
            auto rb = physics::make_dynamic_sphere(BALL_RADIUS, BALL_MASS);
            rb.restitution = BALL_RESTITUTION;
            rb.friction = BALL_FRICTION;
            rb.linear_damping = BALL_LINEAR_DAMPING;
            rb.angular_damping = BALL_ANGULAR_DAMPING;
            world->emplace<physics::RigidBodyComponent>(ball, std::move(rb));

            m_ball_entities.push_back(ball);
        }
    }

    void reset_simulation() {
        log(LogLevel::Info, "[BouncingBalls] Resetting simulation...");

        auto* world = get_world();
        auto* renderer = get_renderer();
        if (!world || !renderer) return;

        // Destroy all scene entities
        for (Entity e : m_ball_entities) world->destroy(e);
        m_ball_entities.clear();

        for (Entity e : m_wall_entities) world->destroy(e);
        m_wall_entities.clear();

        for (Entity e : m_light_entities) world->destroy(e);
        m_light_entities.clear();

        if (m_camera_entity != NullEntity) {
            world->destroy(m_camera_entity);
            m_camera_entity = NullEntity;
        }

        // Destroy materials
        for (auto& mat : m_materials) {
            renderer->destroy_material(mat);
        }
        m_materials.clear();

        // Recreate scene
        create_camera();
        create_lights();
        create_walls();
        create_balls();

        log(LogLevel::Info, "[BouncingBalls] Reset complete.");
    }

    render::MeshHandle m_sphere_mesh;
    render::MeshHandle m_cube_mesh;
    std::vector<render::MaterialHandle> m_materials;

    Entity m_camera_entity = NullEntity;
    std::vector<Entity> m_light_entities;
    std::vector<Entity> m_wall_entities;
    std::vector<Entity> m_ball_entities;
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
