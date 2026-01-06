#pragma once

// Umbrella header for quest system
#include <engine/quest/objective.hpp>
#include <engine/quest/quest.hpp>
#include <engine/quest/quest_manager.hpp>
#include <engine/quest/waypoint.hpp>
#include <engine/quest/quest_components.hpp>

namespace engine::quest {

// System declarations
void quest_system(scene::World& world, double dt);
void waypoint_system(scene::World& world, double dt);
void quest_trigger_system(scene::World& world, double dt);

// Initialization
void register_quest_systems(scene::World& world);

} // namespace engine::quest
