#pragma once

#include <engine/scene/world.hpp>
#include <engine/scene/systems.hpp>

namespace engine::vegetation {

// Forward declarations
class VegetationManager;

// Initialize vegetation rendering systems
// Call this once during engine initialization
void init_vegetation_systems();

// Shutdown vegetation systems
void shutdown_vegetation_systems();

// System functions

// Updates vegetation manager with camera position and frustum
// Phase: Update, Priority: 5
void vegetation_update_system(scene::World& world, double dt);

// Updates grass interactors from entities with GrassInteractorComponent
// Phase: PostUpdate, Priority: 5
void grass_interaction_system(scene::World& world, double dt);

// Renders vegetation (grass and foliage)
// Phase: Render, Priority: 3 (after main render, before post-processing)
void vegetation_render_system(scene::World& world, double dt);

// Renders vegetation shadows
// Phase: PreRender, Priority: 4 (after shadow map setup)
void vegetation_shadow_system(scene::World& world, double dt);

// Register all vegetation systems with the scheduler
void register_vegetation_systems(scene::Scheduler& scheduler);

} // namespace engine::vegetation
