#pragma once

#include <engine/navigation/navmesh.hpp>
#include <engine/core/math.hpp>
#include <memory>
#include <vector>
#include <unordered_map>

// Forward declarations
class dtCrowd;
class dtNavMeshQuery;

namespace engine::navigation {

using namespace engine::core;

// Agent parameters for crowd simulation
struct CrowdAgentParams {
    float radius = 0.5f;           // Agent collision radius
    float height = 2.0f;           // Agent height
    float max_acceleration = 8.0f; // Maximum acceleration
    float max_speed = 3.5f;        // Maximum speed
    float separation_weight = 2.0f; // Weight for separation behavior
    int avoidance_quality = 3;     // 0-3, higher = more accurate but slower
    int obstacle_avoidance_type = 3; // Avoidance algorithm (0-3)
    uint8_t update_flags = 0xFF;   // Which behaviors are enabled
};

// Result from adding an agent to the crowd
struct CrowdAgentHandle {
    int index = -1;  // Internal crowd agent index
    bool valid() const { return index >= 0; }
};

// Crowd state for an agent
struct CrowdAgentState {
    Vec3 position{0.0f};
    Vec3 velocity{0.0f};
    Vec3 desired_velocity{0.0f};
    Vec3 target{0.0f};
    bool has_target = false;
    bool partial_path = false;
    bool at_target = false;
};

// Navigation crowd - manages local avoidance and steering for multiple agents
class NavCrowd {
public:
    NavCrowd();
    ~NavCrowd();

    // Non-copyable
    NavCrowd(const NavCrowd&) = delete;
    NavCrowd& operator=(const NavCrowd&) = delete;

    // Initialize crowd simulation
    bool init(NavMesh* navmesh, int max_agents = 128);
    void shutdown();

    // Check if initialized
    bool is_initialized() const { return m_crowd != nullptr; }

    // Agent management
    CrowdAgentHandle add_agent(const Vec3& position, const CrowdAgentParams& params);
    void remove_agent(CrowdAgentHandle handle);
    void update_agent_params(CrowdAgentHandle handle, const CrowdAgentParams& params);

    // Movement control
    void set_target(CrowdAgentHandle handle, const Vec3& target);
    void set_velocity(CrowdAgentHandle handle, const Vec3& velocity);
    void stop(CrowdAgentHandle handle);

    // Teleport agent (bypasses simulation)
    void warp(CrowdAgentHandle handle, const Vec3& position);

    // Update simulation (call each frame)
    void update(float dt);

    // Query agent state
    CrowdAgentState get_agent_state(CrowdAgentHandle handle) const;
    Vec3 get_agent_position(CrowdAgentHandle handle) const;
    Vec3 get_agent_velocity(CrowdAgentHandle handle) const;
    bool has_reached_target(CrowdAgentHandle handle, float threshold = 0.5f) const;

    // Get active agent count
    int get_active_agent_count() const;
    int get_max_agents() const { return m_max_agents; }

    // Get underlying Detour crowd (for advanced usage)
    dtCrowd* get_detour_crowd() { return m_crowd.get(); }

private:
    // Find nearest point on navmesh
    bool find_nearest_poly(const Vec3& position, Vec3& out_nearest, uint64_t& out_poly) const;

    // Custom deleter for dtCrowd
    struct CrowdDeleter {
        void operator()(dtCrowd* crowd) const;
    };

    struct QueryDeleter {
        void operator()(dtNavMeshQuery* query) const;
    };

    std::unique_ptr<dtCrowd, CrowdDeleter> m_crowd;
    std::unique_ptr<dtNavMeshQuery, QueryDeleter> m_query;
    NavMesh* m_navmesh = nullptr;
    int m_max_agents = 128;
};

} // namespace engine::navigation
