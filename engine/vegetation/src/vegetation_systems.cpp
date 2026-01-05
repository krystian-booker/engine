#include <engine/vegetation/vegetation_systems.hpp>
#include <engine/vegetation/foliage.hpp>
#include <engine/vegetation/grass.hpp>
#include <engine/render/render_systems.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <engine/core/math.hpp>
#include <engine/core/log.hpp>

namespace engine::vegetation {

using namespace engine::core;
using namespace engine::scene;
using namespace engine::render;

// GrassInteractorComponent definition (also registered in component_registration.cpp)
struct GrassInteractorComponent {
    float radius = 1.0f;
    float strength = 1.0f;
    bool enabled = true;
};

// View ID for vegetation rendering (should be configured based on pipeline)
static constexpr uint16_t VEGETATION_VIEW_ID = 5;
static constexpr uint16_t VEGETATION_SHADOW_VIEW_ID = 2;

void init_vegetation_systems() {
    log(LogLevel::Info, "Initializing vegetation systems");
}

void shutdown_vegetation_systems() {
    log(LogLevel::Info, "Shutting down vegetation systems");
}

void vegetation_update_system(World& world, double dt) {
    auto& render_ctx = get_render_context();

    // Only update if we have an active camera
    if (!render_ctx.has_active_camera) {
        return;
    }

    float fdt = static_cast<float>(dt);

    // Build frustum from camera matrices
    Frustum frustum;
    frustum.extract_from_matrix(render_ctx.camera.view_projection);

    // Update vegetation manager
    auto& veg = get_vegetation_manager();
    if (veg.grass().is_initialized() || veg.foliage().is_initialized()) {
        veg.update(fdt, render_ctx.camera.position, frustum);
    }
}

void grass_interaction_system(World& world, double dt) {
    auto& grass = get_grass_system();
    if (!grass.is_initialized()) {
        return;
    }

    // Clear previous frame's interactors (except player)
    grass.clear_interactors();

    // Find all entities with GrassInteractorComponent and WorldTransform
    auto view = world.view<GrassInteractorComponent, WorldTransform>();
    for (auto entity : view) {
        auto& interactor_comp = view.get<GrassInteractorComponent>(entity);
        auto& world_tf = view.get<WorldTransform>(entity);

        if (!interactor_comp.enabled) {
            continue;
        }

        // Calculate velocity from previous position if available
        Vec3 velocity = Vec3(0.0f);
        auto* prev_tf = world.try_get<PreviousTransform>(entity);
        if (prev_tf) {
            velocity = (world_tf.position() - Vec3(prev_tf->matrix[3])) / static_cast<float>(dt);
        }

        // Add as grass interactor
        GrassInteractor gi;
        gi.position = world_tf.position();
        gi.velocity = velocity;
        gi.radius = interactor_comp.radius;
        gi.strength = interactor_comp.strength;

        grass.add_interactor(gi);
    }

    // Also check for player entity (common pattern)
    // Look for entity tagged as player with LocalTransform
    auto player_view = world.view<LocalTransform>();
    for (auto entity : player_view) {
        // Check if this entity has a "Player" tag or similar
        // For now, we'll skip this as it's game-specific
        // Games should call grass.set_player_position() directly
    }
}

void vegetation_render_system(World& world, double dt) {
    auto& render_ctx = get_render_context();

    if (!render_ctx.has_active_camera) {
        return;
    }

    auto& veg = get_vegetation_manager();

    // Render grass
    if (veg.grass().is_initialized()) {
        veg.grass().render(VEGETATION_VIEW_ID);
    }

    // Render foliage
    if (veg.foliage().is_initialized()) {
        veg.foliage().render(VEGETATION_VIEW_ID);
    }
}

void vegetation_shadow_system(World& world, double dt) {
    auto& render_ctx = get_render_context();

    if (!render_ctx.has_active_camera) {
        return;
    }

    auto& veg = get_vegetation_manager();

    // Render grass shadows (usually disabled for performance)
    if (veg.grass().is_initialized() && veg.grass().get_settings().cast_shadows) {
        veg.grass().render_shadow(VEGETATION_SHADOW_VIEW_ID);
    }

    // Render foliage shadows
    if (veg.foliage().is_initialized() && veg.foliage().get_settings().cast_shadows) {
        veg.foliage().render_shadows(VEGETATION_SHADOW_VIEW_ID);
    }
}

void register_vegetation_systems(Scheduler& scheduler) {
    using scene::Phase;

    // Update vegetation visibility and LOD
    scheduler.add(Phase::Update, vegetation_update_system, "vegetation_update", 5);

    // Update grass interaction from entities
    scheduler.add(Phase::PostUpdate, grass_interaction_system, "grass_interaction", 5);

    // Shadow rendering (before main render)
    scheduler.add(Phase::PreRender, vegetation_shadow_system, "vegetation_shadows", 4);

    // Main vegetation rendering
    scheduler.add(Phase::Render, vegetation_render_system, "vegetation_render", 3);

    log(LogLevel::Info, "Registered vegetation ECS systems");
}

} // namespace engine::vegetation
