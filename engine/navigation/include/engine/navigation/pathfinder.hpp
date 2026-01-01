#pragma once

#include <engine/navigation/navmesh.hpp>
#include <engine/navigation/navmesh_builder.hpp>
#include <engine/core/math.hpp>
#include <memory>
#include <vector>

// Forward declaration
class dtNavMeshQuery;
class dtQueryFilter;

namespace engine::navigation {

using namespace engine::core;

// Path query result
struct PathResult {
    std::vector<Vec3> path;          // Smoothed path points
    std::vector<NavPolyRef> polys;   // Polygon path (for debugging)
    bool success = false;
    bool partial = false;            // True if only partial path found

    // Path info
    float total_distance() const;
    bool empty() const { return path.empty(); }
    size_t size() const { return path.size(); }
};

// Raycast result
struct NavRaycastResult {
    bool hit = false;               // True if ray hit a boundary
    Vec3 hit_point{0.0f};          // Point of intersection
    Vec3 hit_normal{0.0f};         // Normal at hit point
    float hit_distance = 0.0f;     // Distance along ray to hit
    NavPolyRef hit_poly = INVALID_NAV_POLY_REF;
};

// Point query result
struct NavPointResult {
    Vec3 point{0.0f};
    NavPolyRef poly = INVALID_NAV_POLY_REF;
    bool valid = false;
};

// Pathfinder - handles all navigation queries
class Pathfinder {
public:
    Pathfinder();
    ~Pathfinder();

    // Non-copyable
    Pathfinder(const Pathfinder&) = delete;
    Pathfinder& operator=(const Pathfinder&) = delete;

    // Initialize with navmesh
    bool init(NavMesh* navmesh, int max_nodes = 2048);
    void shutdown();

    // Set area costs for pathfinding
    void set_area_costs(const NavAreaCosts& costs);
    const NavAreaCosts& get_area_costs() const { return m_area_costs; }

    // Include/exclude areas from pathfinding
    void set_area_enabled(NavAreaType area, bool enabled);
    bool is_area_enabled(NavAreaType area) const;

    // =========================================
    // Path queries
    // =========================================

    // Find path between two world points
    PathResult find_path(const Vec3& start, const Vec3& end);

    // Find path with custom search extents
    PathResult find_path(const Vec3& start, const Vec3& end,
                         const Vec3& search_extents);

    // Find straight path (simplified, without string-pulling)
    PathResult find_straight_path(const Vec3& start, const Vec3& end);

    // =========================================
    // Point queries
    // =========================================

    // Find nearest point on navmesh
    NavPointResult find_nearest_point(const Vec3& point,
                                      float search_radius = 5.0f);

    // Find random point on navmesh
    NavPointResult find_random_point();

    // Find random point within radius of another point
    NavPointResult find_random_point_around(const Vec3& center, float radius);

    // Check if point is on navmesh
    bool is_point_on_navmesh(const Vec3& point, float tolerance = 0.5f);

    // Project point to navmesh surface
    NavPointResult project_point(const Vec3& point,
                                  const Vec3& search_extents = Vec3{2.0f, 4.0f, 2.0f});

    // =========================================
    // Raycasting
    // =========================================

    // Raycast on navmesh (finds first boundary intersection)
    NavRaycastResult raycast(const Vec3& start, const Vec3& end);

    // Check if path between two points is clear (no boundary crossings)
    bool is_path_clear(const Vec3& start, const Vec3& end);

    // =========================================
    // Distance queries
    // =========================================

    // Get distance between two points following navmesh
    float get_path_distance(const Vec3& start, const Vec3& end);

    // Check if point is reachable from another
    bool is_reachable(const Vec3& from, const Vec3& to);

    // =========================================
    // Advanced queries
    // =========================================

    // Find polygons in radius
    std::vector<NavPolyRef> find_polygons_in_radius(const Vec3& center,
                                                     float radius);

    // Get polygon at point
    NavPolyRef get_polygon_at(const Vec3& point,
                              const Vec3& search_extents = Vec3{2.0f, 4.0f, 2.0f});

    // Get polygon center
    Vec3 get_polygon_center(NavPolyRef poly);

    // =========================================
    // State
    // =========================================

    bool is_initialized() const { return m_query != nullptr; }
    NavMesh* get_navmesh() { return m_navmesh; }

private:
    // Create query filter from current settings
    void update_filter();

    NavMesh* m_navmesh = nullptr;

    // Custom deleter for dtNavMeshQuery
    struct QueryDeleter {
        void operator()(dtNavMeshQuery* query) const;
    };

    struct FilterDeleter {
        void operator()(dtQueryFilter* filter) const;
    };

    std::unique_ptr<dtNavMeshQuery, QueryDeleter> m_query;
    std::unique_ptr<dtQueryFilter, FilterDeleter> m_filter;

    NavAreaCosts m_area_costs;
    uint16_t m_include_flags = 0xFFFF;
    uint16_t m_exclude_flags = 0;

    // Scratch buffers for queries
    static constexpr int MAX_PATH_POLYS = 256;
    std::vector<NavPolyRef> m_poly_path;
    std::vector<Vec3> m_straight_path;
};

} // namespace engine::navigation
