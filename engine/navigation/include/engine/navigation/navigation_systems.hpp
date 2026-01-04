#pragma once

#include <engine/scene/world.hpp>

namespace engine::navigation {

class NavMesh;
class Pathfinder;
class NavCrowd;
class NavAgentSystem;
class NavTileCache;

// Initialize global navigation with a navmesh
// Call this after loading/building your navmesh
// The navmesh pointer must remain valid until navigation_shutdown() is called
void navigation_init(NavMesh* navmesh, int max_crowd_agents = 128);

// Initialize navigation with dynamic obstacle support
// Requires navmesh built with build_tiled() or build_tiled_from_world()
void navigation_init_with_obstacles(NavMesh* navmesh, int max_crowd_agents = 128, int max_obstacles = 256);

// Shutdown navigation and release all resources
void navigation_shutdown();

// Check if navigation is initialized
bool navigation_is_initialized();

// ECS system function - updates all NavAgentComponent entities
// Registered automatically by engine in FixedUpdate phase
void navigation_agent_system(scene::World& world, double dt);

// ECS system function - updates all NavObstacleComponent entities
// Registered automatically by engine in FixedUpdate phase (after agent system)
void navigation_obstacle_system(scene::World& world, double dt);

// ECS system function - updates all NavBehaviorComponent entities
// Registered automatically by engine in FixedUpdate phase (before agent system)
void navigation_behavior_system(scene::World& world, double dt);

// Access global instances (for manual pathfinding queries)
Pathfinder* get_pathfinder();
NavCrowd* get_crowd();
NavAgentSystem* get_agent_system();
NavTileCache* get_tile_cache();

// Check if dynamic obstacles are supported
bool navigation_supports_obstacles();

} // namespace engine::navigation
