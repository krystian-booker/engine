#pragma once

#include <engine/scene/systems.hpp>
#include <engine/render/render_pipeline.hpp>
#include <engine/render/billboard.hpp>
#include <vector>

namespace engine::scene {
class World;
class Scheduler;
}

namespace engine::render {

// Skybox data gathered from the scene
struct SkyboxData {
    TextureHandle cubemap;
    float intensity = 1.0f;
    float rotation = 0.0f;
    bool valid = false;
};

// Context shared between render systems during a frame
struct RenderContext {
    RenderPipeline* pipeline = nullptr;
    IRenderer* renderer = nullptr;

    // Gathered data (populated by gather systems, consumed by submit system)
    CameraData camera;
    std::vector<RenderObject> objects;
    std::vector<LightData> lights;
    std::vector<BillboardBatch> billboards;
    SkyboxData skybox;

    // Active camera entity for LOD calculations
    bool has_active_camera = false;

    // Frame delta time (for animation/effects)
    float dt = 0.0f;

    // Clear gathered data for new frame
    void clear() {
        objects.clear();
        lights.clear();
        billboards.clear();
        has_active_camera = false;
        skybox = SkyboxData{};
    }
};

// Get the global render context
RenderContext& get_render_context();

// Initialize render systems with pipeline and renderer
void init_render_systems(RenderPipeline* pipeline, IRenderer* renderer);

// Shutdown render systems
void shutdown_render_systems();

// ============================================================================
// ECS SYSTEM FUNCTIONS
// ============================================================================

// Animation system - evaluates animation state machines and calculates bone matrices
// Phase: Update, Priority: 5
void animation_update_system(scene::World& world, double dt);

// IK system - applies inverse kinematics corrections after animation
// Phase: Update, Priority: 4 (runs after animation_update_system)
void ik_update_system(scene::World& world, double dt);

// Camera controller system - updates follow/orbit cameras and applies effects
// Phase: PostUpdate, Priority: 5
void camera_controller_system(scene::World& world, double dt);

// LOD selection system - calculates LOD levels based on camera distance
// Phase: PreRender, Priority: 10 (runs first in PreRender)
void lod_select_system(scene::World& world, double dt);

// Light gather system - collects lights into render context
// Phase: PreRender, Priority: 8
void light_gather_system(scene::World& world, double dt);

// Skybox gather system - finds skybox component and sets on pipeline
// Phase: PreRender, Priority: 7
void skybox_gather_system(scene::World& world, double dt);

// Billboard gather system - collects billboards for rendering
// Phase: PreRender, Priority: 6
void billboard_gather_system(scene::World& world, double dt);

// Render gather system - frustum culls and builds draw call list
// Phase: PreRender, Priority: 5 (runs after light and LOD systems)
void render_gather_system(scene::World& world, double dt);

// Render submit system - executes the render pipeline
// Phase: Render, Priority: 10
void render_submit_system(scene::World& world, double dt);

// Debug draw system - flushes debug visualization
// Phase: PostRender, Priority: 5
void debug_draw_system(scene::World& world, double dt);

// ============================================================================
// REGISTRATION
// ============================================================================

// Register all render systems with the scheduler
void register_render_systems(scene::Scheduler& scheduler);

} // namespace engine::render
