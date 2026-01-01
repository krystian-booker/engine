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

// Input geometry for navmesh building
struct NavMeshInputGeometry {
    std::vector<Vec3> vertices;
    std::vector<uint32_t> indices;  // Triangle indices (3 per triangle)

    // Optional: area types per triangle (for different traversal costs)
    std::vector<uint8_t> area_types;  // One per triangle, 0 = walkable

    // Bounds (computed automatically if empty)
    AABB bounds;

    // Add triangle mesh
    void add_mesh(const Vec3* verts, size_t vert_count,
                  const uint32_t* inds, size_t index_count,
                  const Mat4& transform = Mat4{1.0f},
                  uint8_t area_type = 0);

    // Clear all geometry
    void clear();

    // Compute bounds from vertices
    void compute_bounds();

    // Get triangle count
    size_t triangle_count() const { return indices.size() / 3; }
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

    std::atomic<bool> m_building{false};
    std::atomic<bool> m_cancel_requested{false};
};

} // namespace engine::navigation
