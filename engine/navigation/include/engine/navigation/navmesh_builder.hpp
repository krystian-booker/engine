#pragma once

#include <engine/navigation/navmesh.hpp>
#include <engine/core/math.hpp>
#include <memory>
#include <vector>
#include <functional>
#include <future>

namespace engine::scene {
    class World;
}

namespace engine::navigation {

using namespace engine::core;

// Build progress callback
using BuildProgressCallback = std::function<void(float progress, const std::string& stage)>;

// Build result
struct NavMeshBuildResult {
    std::unique_ptr<NavMesh> navmesh;
    bool success = false;
    std::string error_message;
    float build_time_ms = 0.0f;

    // Statistics
    int input_vertices = 0;
    int input_triangles = 0;
    int output_polygons = 0;
    int output_tiles = 0;
};

// Area type definitions
enum class NavAreaType : uint8_t {
    Walkable = 0,       // Normal walkable area
    Water = 1,          // Water (higher cost)
    Grass = 2,          // Grass (slightly higher cost)
    Road = 3,           // Road (lower cost)
    Door = 4,           // Door (may be blocked)
    Jump = 5,           // Jump-required area
    NotWalkable = 63    // Blocked
};

// Area costs for pathfinding
struct NavAreaCosts {
    float costs[64] = {1.0f};  // Default cost 1.0 for all areas

    NavAreaCosts() {
        for (int i = 0; i < 64; ++i) costs[i] = 1.0f;
        costs[static_cast<int>(NavAreaType::NotWalkable)] = 1000000.0f;
    }

    void set_cost(NavAreaType area, float cost) {
        costs[static_cast<int>(area)] = cost;
    }

    float get_cost(NavAreaType area) const {
        return costs[static_cast<int>(area)];
    }
};

// Off-mesh connection flags
enum class OffMeshConnectionFlags : uint16_t {
    None = 0,
    Bidirectional = 1 << 0,  // Can traverse in both directions
    Jump = 1 << 1,           // Requires jump animation
    Ladder = 1 << 2,         // Ladder traversal
    Door = 1 << 3,           // Door (may be locked/unlocked)
    Teleport = 1 << 4,       // Instant teleportation
    Climb = 1 << 5           // Climbing animation required
};

inline OffMeshConnectionFlags operator|(OffMeshConnectionFlags a, OffMeshConnectionFlags b) {
    return static_cast<OffMeshConnectionFlags>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}

inline OffMeshConnectionFlags operator&(OffMeshConnectionFlags a, OffMeshConnectionFlags b) {
    return static_cast<OffMeshConnectionFlags>(static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
}

inline bool has_flag(OffMeshConnectionFlags flags, OffMeshConnectionFlags flag) {
    return (static_cast<uint16_t>(flags) & static_cast<uint16_t>(flag)) != 0;
}

// Off-mesh connection definition for linking disconnected navmesh areas
struct OffMeshConnection {
    Vec3 start{0.0f};                // Start position
    Vec3 end{0.0f};                  // End position
    float radius = 0.5f;             // Connection radius (agent must be within this to use)
    OffMeshConnectionFlags flags = OffMeshConnectionFlags::Bidirectional;
    NavAreaType area = NavAreaType::Walkable;  // Area type for cost calculation
    uint32_t user_id = 0;            // Game-specific identifier (e.g., door entity ID)
};

// Input geometry for navmesh building
struct NavMeshInputGeometry {
    std::vector<Vec3> vertices;
    std::vector<uint32_t> indices;  // Triangle indices (3 per triangle)

    // Optional: area types per triangle (for different traversal costs)
    std::vector<uint8_t> area_types;  // One per triangle, 0 = walkable

    // Off-mesh connections (ladders, jumps, doors, etc.)
    std::vector<OffMeshConnection> off_mesh_connections;

    // Bounds (computed automatically if empty)
    AABB bounds;

    // Add triangle mesh
    void add_mesh(const Vec3* verts, size_t vert_count,
                  const uint32_t* inds, size_t index_count,
                  const Mat4& transform = Mat4{1.0f},
                  uint8_t area_type = 0);

    // Add off-mesh connection
    void add_off_mesh_connection(const OffMeshConnection& connection);

    // Add off-mesh connection (convenience overload)
    void add_off_mesh_connection(const Vec3& start, const Vec3& end,
                                  float radius = 0.5f,
                                  OffMeshConnectionFlags flags = OffMeshConnectionFlags::Bidirectional,
                                  NavAreaType area = NavAreaType::Walkable,
                                  uint32_t user_id = 0);

    // Clear all geometry
    void clear();

    // Compute bounds from vertices
    void compute_bounds();

    // Get triangle count
    size_t triangle_count() const { return indices.size() / 3; }

    // Get off-mesh connection count
    size_t off_mesh_count() const { return off_mesh_connections.size(); }
};



// Component for entities that contribute to navmesh building
struct NavMeshSource {
    std::vector<Vec3> vertices;
    std::vector<uint32_t> indices;   // Triangle indices (3 per triangle)
    uint8_t area_type = 0;           // 0 = walkable, see NavAreaType enum
    bool enabled = true;             // Include in navmesh builds
};

// Component for off-mesh link entities (ladders, doors, jump points, etc.)
struct OffMeshLinkComponent {
    Vec3 start_offset{0.0f};         // Start position offset from entity transform
    Vec3 end_offset{0.0f, 0.0f, 2.0f}; // End position offset from entity transform
    float radius = 0.5f;             // Connection radius
    OffMeshConnectionFlags flags = OffMeshConnectionFlags::Bidirectional;
    NavAreaType area = NavAreaType::Walkable;
    bool enabled = true;
};

// Navmesh builder
class NavMeshBuilder {
public:
    NavMeshBuilder();
    ~NavMeshBuilder();

    // Build from raw geometry
    NavMeshBuildResult build(
        const NavMeshInputGeometry& geometry,
        const NavMeshSettings& settings,
        BuildProgressCallback progress = nullptr);

    // Build from scene world (extracts static geometry with MeshRenderer components)
    NavMeshBuildResult build_from_world(
        scene::World& world,
        const NavMeshSettings& settings,
        uint32_t layer_mask = 0xFFFFFFFF,
        BuildProgressCallback progress = nullptr);

    // Async building (returns immediately, result via future)
    std::future<NavMeshBuildResult> build_async(
        NavMeshInputGeometry geometry,
        NavMeshSettings settings,
        BuildProgressCallback progress = nullptr);

    // Build with tile cache support (required for dynamic obstacles)
    // Note: settings.use_tiles will be forced to true
    NavMeshBuildResult build_tiled(
        const NavMeshInputGeometry& geometry,
        const NavMeshSettings& settings,
        BuildProgressCallback progress = nullptr);

    // Build from world with tile cache support
    NavMeshBuildResult build_tiled_from_world(
        scene::World& world,
        const NavMeshSettings& settings,
        uint32_t layer_mask = 0xFFFFFFFF,
        BuildProgressCallback progress = nullptr);

    // Cancel ongoing async build
    void cancel_build();

    // Check if build is in progress
    bool is_building() const { return m_building; }

private:
    // Internal build implementation
    NavMeshBuildResult build_internal(
        const NavMeshInputGeometry& geometry,
        const NavMeshSettings& settings,
        BuildProgressCallback progress);

    // Internal tiled build with tile cache layer generation
    NavMeshBuildResult build_tiled_internal(
        const NavMeshInputGeometry& geometry,
        const NavMeshSettings& settings,
        BuildProgressCallback progress);

    std::atomic<bool> m_building{false};
    std::atomic<bool> m_cancel_requested{false};
};

} // namespace engine::navigation
