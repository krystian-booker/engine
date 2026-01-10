#pragma once

#include <engine/scene/world.hpp>

namespace engine::companion {

// ============================================================================
// Companion AI Systems
// ============================================================================

// Main companion follow system - handles movement toward leader/formation position
// Phase: FixedUpdate (after AI, before physics)
void companion_follow_system(scene::World& world, double dt);

// Companion combat system - handles enemy detection and engagement
// Phase: FixedUpdate (after follow system)
void companion_combat_system(scene::World& world, double dt);

// Companion command system - processes pending commands
// Phase: Update
void companion_command_system(scene::World& world, double dt);

// Companion teleport system - handles teleporting companions who are too far
// Phase: PostUpdate
void companion_teleport_system(scene::World& world, double dt);

// ============================================================================
// Registration
// ============================================================================

// Register all companion systems with the world scheduler
void register_companion_systems(scene::World& world);

} // namespace engine::companion
