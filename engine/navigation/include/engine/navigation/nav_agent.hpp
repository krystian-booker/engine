#pragma once

#include <engine/navigation/pathfinder.hpp>
#include <engine/core/math.hpp>
#include <vector>
#include <cstdint>
#include <functional>

namespace engine::scene {
    class World;
}

namespace engine::navigation {

using namespace engine::core;

// Agent movement state
enum class NavAgentState : uint8_t {
    Idle,           // No destination set
    Moving,         // Following path
    Waiting,        // Waiting for obstacle/crowd
    Arrived,        // Reached destination
    Failed          // Path not found or unreachable
};

// Agent event types for callbacks
enum class NavAgentEvent : uint8_t {
    Arrived,        // Agent reached destination
    Failed,         // Pathfinding failed or path is unreachable
    PathBlocked,    // Path became blocked during movement
    Waiting,        // Agent is waiting (crowd congestion)
    Rerouted        // Agent recalculated path due to obstacle
};

// Agent avoidance quality
enum class AvoidanceQuality : uint8_t {
    None,           // No avoidance
    Low,            // Simple avoidance
    Medium,         // Moderate quality
    High            // High quality (more samples)
};

// Navigation agent component
struct NavAgentComponent {
    // Movement settings
    float speed = 3.5f;              // Maximum speed (units/sec)
    float acceleration = 8.0f;       // Acceleration (units/sec^2)
    float deceleration = 10.0f;      // Deceleration when stopping
    float turning_speed = 360.0f;    // Degrees per second

    // Path following
    float path_radius = 0.5f;        // Radius for path following
    float stopping_distance = 0.1f;  // Distance at which agent stops
    float height = 2.0f;             // Agent height for ground detection

    // Avoidance
    float avoidance_radius = 0.5f;   // Radius for collision avoidance
    AvoidanceQuality avoidance = AvoidanceQuality::Medium;
    int avoidance_priority = 50;     // 0-99, lower = higher priority

    // Crowd simulation settings
    bool use_crowd = true;           // Use crowd simulation for local avoidance
    float separation_weight = 2.0f;  // Weight for separation behavior in crowd

    // Path settings
    bool auto_repath = true;         // Automatically recalculate on failure
    float repath_interval = 0.5f;    // Minimum time between repaths
    float corner_threshold = 0.1f;   // Distance to trigger corner rounding

    // Area traversal
    NavAreaCosts area_costs;         // Custom area costs for this agent

    // Current state (runtime)
    NavAgentState state = NavAgentState::Idle;
    Vec3 target{0.0f};               // Current target position
    Vec3 velocity{0.0f};             // Current velocity
    float current_speed = 0.0f;      // Current speed magnitude
    bool has_target = false;

    // Path data (runtime - used when not using crowd)
    std::vector<Vec3> path;          // Current path
    size_t path_index = 0;           // Current path segment
    float path_distance = 0.0f;      // Distance remaining on path
    float time_since_repath = 0.0f;  // Time since last path calculation

    // Crowd agent data (runtime - used when using crowd)
    int crowd_agent_index = -1;      // Index in crowd (-1 if not registered)

    // Debug
    bool debug_draw = false;         // Draw path and state

    // Callbacks (runtime - not serialized)
    std::function<void(NavAgentEvent)> on_event;  // Called on state changes
    NavAgentState previous_state = NavAgentState::Idle;  // For change detection
};

// Forward declaration
class NavCrowd;

// Nav agent system - updates all agents
class NavAgentSystem {
public:
    NavAgentSystem();
    ~NavAgentSystem();

    // Initialize with pathfinder (simple mode without crowd)
    void init(Pathfinder* pathfinder);

    // Initialize with pathfinder and crowd (enables local avoidance)
    void init(Pathfinder* pathfinder, NavCrowd* crowd);

    void shutdown();

    // Set target for agent
    void set_destination(scene::World& world, uint32_t entity_id, const Vec3& target);

    // Stop agent
    void stop(scene::World& world, uint32_t entity_id);

    // Warp agent to position (no pathfinding)
    void warp(scene::World& world, uint32_t entity_id, const Vec3& position);

    // Update all agents (call each frame)
    void update(scene::World& world, float dt);

    // Check if agent has reached destination
    bool has_arrived(scene::World& world, uint32_t entity_id) const;

    // Get remaining distance to target
    float get_remaining_distance(scene::World& world, uint32_t entity_id) const;

    // Set event callback for agent
    void set_callback(scene::World& world, uint32_t entity_id,
                      std::function<void(NavAgentEvent)> callback);

    // Clear event callback for agent
    void clear_callback(scene::World& world, uint32_t entity_id);

    // Crowd access
    NavCrowd* get_crowd() { return m_crowd; }
    bool has_crowd() const { return m_crowd != nullptr; }

    // Crowd simulation settings
    void set_max_agents(int max_agents);
    int get_max_agents() const { return m_max_agents; }

private:
    // Update single agent (simple mode)
    void update_agent_simple(NavAgentComponent& agent, Vec3& position, float dt);

    // Update single agent (crowd mode)
    void update_agent_crowd(NavAgentComponent& agent, Vec3& position, float dt);

    // Register agent with crowd
    void register_crowd_agent(NavAgentComponent& agent, const Vec3& position);

    // Unregister agent from crowd
    void unregister_crowd_agent(NavAgentComponent& agent);

    // Calculate path for agent
    void calculate_path(NavAgentComponent& agent, const Vec3& position);

    // Follow path logic
    void follow_path(NavAgentComponent& agent, Vec3& position, float dt);

    // Smooth path corners
    void smooth_path(NavAgentComponent& agent);

    Pathfinder* m_pathfinder = nullptr;
    NavCrowd* m_crowd = nullptr;
    int m_max_agents = 128;
};

} // namespace engine::navigation
