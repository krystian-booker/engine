#pragma once

#include <engine/core/math.hpp>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <DetourNavMesh.h>

namespace engine::navigation {

using namespace engine::core;

// Navmesh build settings
struct NavMeshSettings {
    // Rasterization
    float cell_size = 0.3f;          // xz cell size (smaller = more detail, more memory)
    float cell_height = 0.2f;        // y cell size

    // Agent properties
    float agent_height = 2.0f;       // Minimum ceiling height
    float agent_radius = 0.6f;       // Agent collision radius
    float agent_max_climb = 0.9f;    // Maximum step height
    float agent_max_slope = 45.0f;   // Maximum walkable slope (degrees)

    // Region settings
    int min_region_area = 8;         // Minimum region size (in cells)
    int merge_region_area = 20;      // Regions smaller than this will be merged

    // Edge settings
    float max_edge_length = 12.0f;   // Maximum edge length
    float max_edge_error = 1.3f;     // Maximum deviation from source geometry

    // Detail mesh settings
    float detail_sample_distance = 6.0f;
    float detail_sample_max_error = 1.0f;

    // Polygon settings
    int max_verts_per_poly = 6;      // 3-6 (higher = fewer polygons, more complex)

    // Tile settings (for tiled navmesh)
    bool use_tiles = false;
    float tile_size = 48.0f;         // In world units
};

// Polygon reference type
using NavPolyRef = dtPolyRef;

// Invalid polygon reference
constexpr NavPolyRef INVALID_NAV_POLY_REF = 0;

// Navigation mesh wrapper
class NavMesh {
public:
    NavMesh();
    ~NavMesh();

    // Non-copyable, movable
    NavMesh(const NavMesh&) = delete;
    NavMesh& operator=(const NavMesh&) = delete;
    NavMesh(NavMesh&& other) noexcept;
    NavMesh& operator=(NavMesh&& other) noexcept;

    // Serialization
    bool load(const std::string& path);
    bool save(const std::string& path) const;

    // Load from memory
    bool load_from_memory(const uint8_t* data, size_t size);

    // Get binary data for serialization
    std::vector<uint8_t> get_binary_data() const;

    // Check if valid
    bool is_valid() const { return m_navmesh != nullptr; }

    // Get underlying Detour navmesh (for advanced usage)
    dtNavMesh* get_detour_navmesh() { return m_navmesh.get(); }
    const dtNavMesh* get_detour_navmesh() const { return m_navmesh.get(); }

    // Statistics
    int get_tile_count() const;
    int get_polygon_count() const;
    int get_vertex_count() const;

    // Get navmesh bounds
    AABB get_bounds() const;

    // Debug visualization data
    struct DebugVertex {
        Vec3 position;
        Vec4 color;
    };

    // Get vertices for debug rendering (triangles)
    std::vector<DebugVertex> get_debug_geometry() const;

private:
    friend class NavMeshBuilder;

    // Custom deleter for dtNavMesh
    struct NavMeshDeleter {
        void operator()(dtNavMesh* mesh) const;
    };

    std::unique_ptr<dtNavMesh, NavMeshDeleter> m_navmesh;
    NavMeshSettings m_settings;
};

} // namespace engine::navigation
