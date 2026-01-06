#pragma once

// Umbrella header for engine::ai module

#include <engine/ai/blackboard.hpp>
#include <engine/ai/behavior_tree.hpp>
#include <engine/ai/bt_composites.hpp>
#include <engine/ai/bt_decorators.hpp>
#include <engine/ai/bt_nodes.hpp>
#include <engine/ai/perception.hpp>
#include <engine/ai/ai_components.hpp>

namespace engine::ai {

// ============================================================================
// ECS Systems
// ============================================================================

// Main AI update system (FixedUpdate phase)
void ai_behavior_system(scene::World& world, double dt);

// Combat AI system (FixedUpdate phase, after behavior)
void ai_combat_system(scene::World& world, double dt);

// Patrol system (FixedUpdate phase)
void ai_patrol_system(scene::World& world, double dt);

// ============================================================================
// Initialization
// ============================================================================

// Register all AI components with reflection
void register_ai_components();

// Register all AI systems with scheduler
void register_ai_systems(scene::World& world);

} // namespace engine::ai
