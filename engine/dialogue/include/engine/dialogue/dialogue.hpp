#pragma once

// Umbrella header for dialogue system
#include <engine/dialogue/dialogue_node.hpp>
#include <engine/dialogue/dialogue_graph.hpp>
#include <engine/dialogue/dialogue_player.hpp>
#include <engine/dialogue/dialogue_components.hpp>

namespace engine::dialogue {

// Forward declarations
class DialogueGraph;
class DialoguePlayer;
class DialogueLibrary;

// System declarations
void dialogue_system(scene::World& world, double dt);
void dialogue_trigger_system(scene::World& world, double dt);
void barks_system(scene::World& world, double dt);

// Initialization
void register_dialogue_systems(scene::World& world);

} // namespace engine::dialogue
