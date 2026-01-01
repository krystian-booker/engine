#pragma once

namespace engine::scene {
    class World;
}

// ECS System functions
// Systems are functions that operate on entities with specific components

// Player movement system
// Processes all entities with PlayerController and LocalTransform components
void player_movement_system(engine::scene::World& world, double dt);

// Health system
// Processes all entities with Health component
void health_system(engine::scene::World& world, double dt);
