#pragma once

#include <engine/navigation/navmesh.hpp>
#include <engine/core/math.hpp>
#include <memory>
#include <cstdint>

// Forward declarations for Detour types
class dtTileCache;
class dtNavMesh;
struct dtTileCacheAlloc;
struct dtTileCacheCompressor;
struct dtTileCacheMeshProcess;

namespace engine::navigation {

using namespace engine::core;

// Obstacle handle (wrapper around dtObstacleRef)
struct NavObstacleHandle {
    uint32_t id = 0;
    bool valid() const { return id != 0; }
};

// Obstacle shape types supported by TileCache
enum class ObstacleShape : uint8_t {
    Cylinder,      // Circular footprint (pos, radius, height)
    Box,           // Axis-aligned box (center, half_extents)
    OrientedBox    // Rotated box (center, half_extents, y_rotation)
};

// Settings for tile cache
struct NavTileCacheSettings {
    int max_obstacles = 256;
    int max_layers = 32;              // Max layers per tile
    float max_simplification_error = 1.3f;
};

// Result from adding obstacle
struct ObstacleResult {
    NavObstacleHandle handle;
    bool success = false;
    std::string error_message;
};

// Tile cache wrapper for runtime dynamic obstacles
class NavTileCache {
public:
    NavTileCache();
    ~NavTileCache();

    // Non-copyable
    NavTileCache(const NavTileCache&) = delete;
    NavTileCache& operator=(const NavTileCache&) = delete;

    // Initialize with a tiled navmesh
    // Note: navmesh must have been built with use_tiles=true
    bool init(NavMesh* navmesh, const NavTileCacheSettings& settings = {});
    void shutdown();

    // Check if initialized
    bool is_initialized() const { return m_tile_cache != nullptr; }

    // Add obstacles
    ObstacleResult add_cylinder(const Vec3& position, float radius, float height);
    ObstacleResult add_box(const Vec3& center, const Vec3& half_extents);
    ObstacleResult add_oriented_box(const Vec3& center, const Vec3& half_extents, float y_rotation_radians);

    // Remove obstacle
    void remove_obstacle(NavObstacleHandle handle);

    // Update obstacle (convenience - removes old and adds new)
    ObstacleResult update_cylinder(NavObstacleHandle& handle, const Vec3& position, float radius, float height);
    ObstacleResult update_box(NavObstacleHandle& handle, const Vec3& center, const Vec3& half_extents);
    ObstacleResult update_oriented_box(NavObstacleHandle& handle, const Vec3& center, const Vec3& half_extents, float y_rotation_radians);

    // Process pending obstacle changes
    // Must be called each frame to apply obstacle modifications to navmesh
    // Returns true if all updates are complete
    bool update(float dt);

    // Query
    int get_obstacle_count() const { return m_active_obstacles; }
    int get_max_obstacles() const { return m_settings.max_obstacles; }

    // Access underlying cache (advanced usage)
    dtTileCache* get_detour_tile_cache() { return m_tile_cache.get(); }

private:
    // Custom deleters
    struct TileCacheDeleter {
        void operator()(dtTileCache* tc) const;
    };
    struct AllocDeleter {
        void operator()(dtTileCacheAlloc* alloc) const;
    };
    struct CompressorDeleter {
        void operator()(dtTileCacheCompressor* comp) const;
    };
    struct MeshProcessDeleter {
        void operator()(dtTileCacheMeshProcess* proc) const;
    };

    std::unique_ptr<dtTileCache, TileCacheDeleter> m_tile_cache;
    std::unique_ptr<dtTileCacheAlloc, AllocDeleter> m_alloc;
    std::unique_ptr<dtTileCacheCompressor, CompressorDeleter> m_compressor;
    std::unique_ptr<dtTileCacheMeshProcess, MeshProcessDeleter> m_mesh_process;

    NavMesh* m_navmesh = nullptr;
    NavTileCacheSettings m_settings;
    int m_active_obstacles = 0;
};

} // namespace engine::navigation
