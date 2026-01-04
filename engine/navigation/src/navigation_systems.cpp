#include <engine/navigation/navigation_systems.hpp>
#include <engine/navigation/navmesh.hpp>
#include <engine/navigation/pathfinder.hpp>
#include <engine/navigation/nav_crowd.hpp>
#include <engine/navigation/nav_agent.hpp>
#include <engine/navigation/nav_tile_cache.hpp>
#include <engine/navigation/nav_obstacle.hpp>
#include <engine/scene/transform.hpp>
#include <engine/core/log.hpp>

#include <memory>
#include <cmath>

namespace engine::navigation {

// Global navigation state
static NavMesh* g_navmesh = nullptr;
static std::unique_ptr<Pathfinder> g_pathfinder;
static std::unique_ptr<NavCrowd> g_crowd;
static std::unique_ptr<NavAgentSystem> g_agent_system;
static std::unique_ptr<NavTileCache> g_tile_cache;

void navigation_init(NavMesh* navmesh, int max_crowd_agents) {
    if (!navmesh || !navmesh->is_valid()) {
        core::log(core::LogLevel::Error, "navigation_init: Invalid navmesh provided");
        return;
    }

    // Shutdown existing navigation if any
    navigation_shutdown();

    g_navmesh = navmesh;

    // Create and initialize pathfinder
    g_pathfinder = std::make_unique<Pathfinder>();
    if (!g_pathfinder->init(navmesh)) {
        core::log(core::LogLevel::Error, "navigation_init: Failed to initialize pathfinder");
        navigation_shutdown();
        return;
    }

    // Create and initialize crowd
    g_crowd = std::make_unique<NavCrowd>();
    if (!g_crowd->init(navmesh, max_crowd_agents)) {
        core::log(core::LogLevel::Error, "navigation_init: Failed to initialize crowd");
        navigation_shutdown();
        return;
    }

    // Create and initialize agent system with crowd mode
    g_agent_system = std::make_unique<NavAgentSystem>();
    g_agent_system->init(g_pathfinder.get(), g_crowd.get());
    g_agent_system->set_max_agents(max_crowd_agents);

    core::log(core::LogLevel::Info, "Navigation initialized (max {} agents)", max_crowd_agents);
}

void navigation_init_with_obstacles(NavMesh* navmesh, int max_crowd_agents, int max_obstacles) {
    // First do standard initialization
    navigation_init(navmesh, max_crowd_agents);

    if (!navigation_is_initialized()) {
        return;
    }

    // Initialize tile cache for dynamic obstacles
    if (navmesh->supports_tile_cache()) {
        g_tile_cache = std::make_unique<NavTileCache>();
        NavTileCacheSettings cache_settings;
        cache_settings.max_obstacles = max_obstacles;

        if (!g_tile_cache->init(navmesh, cache_settings)) {
            core::log(core::LogLevel::Warn, "navigation_init_with_obstacles: Failed to initialize tile cache");
            g_tile_cache.reset();
        } else {
            core::log(core::LogLevel::Info, "Navigation tile cache initialized (max {} obstacles)", max_obstacles);
        }
    } else {
        core::log(core::LogLevel::Warn, "navigation_init_with_obstacles: Navmesh does not support tile cache");
    }
}

void navigation_shutdown() {
    if (g_tile_cache) {
        g_tile_cache->shutdown();
        g_tile_cache.reset();
    }

    if (g_agent_system) {
        g_agent_system->shutdown();
        g_agent_system.reset();
    }

    if (g_crowd) {
        g_crowd->shutdown();
        g_crowd.reset();
    }

    if (g_pathfinder) {
        g_pathfinder->shutdown();
        g_pathfinder.reset();
    }

    g_navmesh = nullptr;

    core::log(core::LogLevel::Info, "Navigation shutdown");
}

bool navigation_is_initialized() {
    return g_navmesh != nullptr &&
           g_pathfinder != nullptr &&
           g_pathfinder->is_initialized();
}

void navigation_agent_system(scene::World& world, double dt) {
    if (!g_agent_system) {
        return;
    }

    g_agent_system->update(world, static_cast<float>(dt));
}

Pathfinder* get_pathfinder() {
    return g_pathfinder.get();
}

NavCrowd* get_crowd() {
    return g_crowd.get();
}

NavAgentSystem* get_agent_system() {
    return g_agent_system.get();
}

NavTileCache* get_tile_cache() {
    return g_tile_cache.get();
}

bool navigation_supports_obstacles() {
    return g_tile_cache != nullptr && g_tile_cache->is_initialized();
}

// Helper to extract Y rotation from a 4x4 transform matrix
static float extract_y_rotation(const Mat4& matrix) {
    // Extract rotation around Y axis from the matrix
    // Assuming the matrix is a standard TRS matrix
    Vec3 forward(matrix[2][0], matrix[2][1], matrix[2][2]);
    return std::atan2(forward.x, forward.z);
}

void navigation_obstacle_system(scene::World& world, double dt) {
    if (!g_tile_cache || !g_tile_cache->is_initialized()) {
        return;
    }

    // Query entities with NavObstacleComponent and WorldTransform
    auto view = world.view<NavObstacleComponent, scene::WorldTransform>();

    for (auto entity : view) {
        auto& obstacle = world.get<NavObstacleComponent>(entity);
        auto& transform = world.get<scene::WorldTransform>(entity);

        if (!obstacle.enabled) {
            // Remove if was previously added
            if (obstacle.handle.valid()) {
                g_tile_cache->remove_obstacle(obstacle.handle);
                obstacle.handle = NavObstacleHandle{};
            }
            continue;
        }

        Vec3 world_pos = Vec3(transform.matrix[3]) + obstacle.offset;
        float y_rotation = extract_y_rotation(transform.matrix);

        // Check if update needed (position or rotation changed significantly)
        bool needs_update = obstacle.needs_update ||
                           glm::distance(world_pos, obstacle.last_position) > 0.05f ||
                           std::abs(y_rotation - obstacle.last_y_rotation) > 0.01f;

        if (needs_update) {
            // Add or update obstacle based on shape
            ObstacleResult result;
            switch (obstacle.shape) {
                case ObstacleShape::Cylinder:
                    result = g_tile_cache->update_cylinder(obstacle.handle, world_pos,
                                                           obstacle.cylinder_radius,
                                                           obstacle.cylinder_height);
                    break;
                case ObstacleShape::Box:
                    result = g_tile_cache->update_box(obstacle.handle, world_pos,
                                                      obstacle.half_extents);
                    break;
                case ObstacleShape::OrientedBox:
                    result = g_tile_cache->update_oriented_box(obstacle.handle, world_pos,
                                                               obstacle.half_extents,
                                                               y_rotation);
                    break;
            }

            if (result.success) {
                obstacle.handle = result.handle;
            }

            obstacle.needs_update = false;
            obstacle.last_position = world_pos;
            obstacle.last_y_rotation = y_rotation;
        }
    }

    // Process pending tile cache updates
    g_tile_cache->update(static_cast<float>(dt));
}

} // namespace engine::navigation
