#include <engine/terrain/terrain.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/systems.hpp>
#include <engine/scene/transform.hpp>
#include <engine/scene/render_components.hpp>
#include <engine/core/log.hpp>

namespace engine::terrain {

using namespace engine::core;
using namespace engine::scene;

// ============================================================================
// Terrain Update System
// ============================================================================

void terrain_update_system(scene::World& world, double dt) {
    using namespace engine::scene;

    auto& terrain_manager = get_terrain_manager();

    // Find active camera for LOD and frustum culling
    Vec3 camera_position{0.0f};
    Frustum frustum;
    bool found_camera = false;

    auto camera_view = world.view<Camera, WorldTransform>();
    for (auto entity : camera_view) {
        auto& cam = camera_view.get<Camera>(entity);
        if (cam.active) {
            auto& world_tf = camera_view.get<WorldTransform>(entity);
            camera_position = world_tf.position();

            // Build frustum from camera
            Mat4 proj = cam.projection();
            Mat4 view = glm::inverse(world_tf.matrix);
            Mat4 view_proj = proj * view;
            frustum.extract_from_matrix(view_proj);

            found_camera = true;
            break;
        }
    }

    if (!found_camera) {
        return;
    }

    // Sync TerrainComponent entities with TerrainManager
    auto terrain_view = world.view<TerrainComponent, WorldTransform>();
    for (auto entity : terrain_view) {
        auto& terrain_comp = terrain_view.get<TerrainComponent>(entity);
        auto& world_tf = terrain_view.get<WorldTransform>(entity);

        // Update cached pointer
        if (!terrain_comp.terrain_ptr && terrain_comp.terrain_id != UINT32_MAX) {
            terrain_comp.terrain_ptr = terrain_manager.get_terrain(terrain_comp.terrain_id);
        }

        // Sync position from entity transform
        if (terrain_comp.terrain_ptr) {
            terrain_comp.terrain_ptr->set_position(world_tf.position());
        }
    }

    // Update all terrains (LOD, visibility culling)
    terrain_manager.update(static_cast<float>(dt), camera_position, frustum);
}

// ============================================================================
// Terrain Render System
// ============================================================================

void terrain_render_system(scene::World& world, double /*dt*/) {
    using namespace engine::scene;

    auto& terrain_manager = get_terrain_manager();

    // Render all terrains to main view
    constexpr uint16_t MAIN_VIEW_ID = 0;
    terrain_manager.render(MAIN_VIEW_ID);
}

// ============================================================================
// Terrain Shadow Render System
// ============================================================================

void terrain_shadow_render_system(scene::World& world, double /*dt*/, uint16_t shadow_view_id) {
    auto& terrain_manager = get_terrain_manager();
    terrain_manager.render_shadows(shadow_view_id);
}

// ============================================================================
// Terrain Physics Sync System
// ============================================================================

void terrain_physics_sync_system(scene::World& world, double /*dt*/) {
    auto& terrain_manager = get_terrain_manager();

    // Rebuild physics bodies for terrains that have been modified
    auto terrain_ids = terrain_manager.get_all_terrain_ids();
    for (uint32_t id : terrain_ids) {
        Terrain* terrain = terrain_manager.get_terrain(id);
        if (terrain && terrain->is_valid()) {
            // Physics rebuild is handled internally by Terrain when marked dirty
            // This system exists for any additional physics sync needed
        }
    }
}

// ============================================================================
// System Initialization and Registration
// ============================================================================

void init_terrain_systems() {
    log(LogLevel::Info, "Initializing terrain systems");
}

void shutdown_terrain_systems() {
    log(LogLevel::Info, "Shutting down terrain systems");
    get_terrain_manager().destroy_all();
}

void register_terrain_systems(Scheduler& scheduler) {
    // Update terrain visibility, LOD, and position sync
    scheduler.add(Phase::Update, terrain_update_system, "terrain_update", 4);

    // Physics sync (after physics simulation)
    scheduler.add(Phase::PostUpdate, terrain_physics_sync_system, "terrain_physics_sync", 5);

    // Shadow rendering (before main render)
    scheduler.add(Phase::PreRender, [](World& world, double dt) {
        // Shadow view ID typically configured by render pipeline
        constexpr uint16_t TERRAIN_SHADOW_VIEW_ID = 2;
        terrain_shadow_render_system(world, dt, TERRAIN_SHADOW_VIEW_ID);
    }, "terrain_shadows", 4);

    // Main terrain rendering
    scheduler.add(Phase::Render, terrain_render_system, "terrain_render", 2);

    log(LogLevel::Info, "Registered terrain ECS systems");
}

} // namespace engine::terrain
