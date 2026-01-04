#include <engine/navigation/nav_tile_cache.hpp>
#include <engine/core/log.hpp>

#include <DetourTileCache.h>
#include <DetourTileCacheBuilder.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourCommon.h>

#include <cstring>

namespace engine::navigation {

// Simple linear allocator for tile cache
class LinearAllocator : public dtTileCacheAlloc {
public:
    LinearAllocator(const size_t cap) : m_capacity(cap), m_top(0), m_high(0) {
        m_buffer = static_cast<unsigned char*>(dtAlloc(cap, DT_ALLOC_PERM));
    }

    ~LinearAllocator() override {
        dtFree(m_buffer);
    }

    void reset() override {
        m_high = std::max(m_high, m_top);
        m_top = 0;
    }

    void* alloc(const size_t size) override {
        if (!m_buffer) return nullptr;
        if (m_top + size > m_capacity) return nullptr;
        unsigned char* mem = &m_buffer[m_top];
        m_top += size;
        return mem;
    }

    void free(void* /*ptr*/) override {
        // Linear allocator doesn't support individual frees
    }

private:
    unsigned char* m_buffer = nullptr;
    size_t m_capacity = 0;
    size_t m_top = 0;
    size_t m_high = 0;
};

// Fast LZ compressor for tile cache
class FastLZCompressor : public dtTileCacheCompressor {
public:
    int maxCompressedSize(const int bufferSize) override {
        return static_cast<int>(static_cast<double>(bufferSize) * 1.05);
    }

    dtStatus compress(const unsigned char* buffer, const int bufferSize,
                      unsigned char* compressed, const int maxCompressedSize,
                      int* compressedSize) override {
        // For simplicity, use no compression (copy data as-is)
        // In production, you'd integrate LZ4 or similar
        if (bufferSize > maxCompressedSize) return DT_FAILURE;
        std::memcpy(compressed, buffer, bufferSize);
        *compressedSize = bufferSize;
        return DT_SUCCESS;
    }

    dtStatus decompress(const unsigned char* compressed, const int compressedSize,
                        unsigned char* buffer, const int maxBufferSize,
                        int* bufferSize) override {
        if (compressedSize > maxBufferSize) return DT_FAILURE;
        std::memcpy(buffer, compressed, compressedSize);
        *bufferSize = compressedSize;
        return DT_SUCCESS;
    }
};

// Mesh processor for tile cache
class MeshProcess : public dtTileCacheMeshProcess {
public:
    void process(struct dtNavMeshCreateParams* params,
                 unsigned char* polyAreas, unsigned short* polyFlags) override {
        // Set default flags for all polygons
        for (int i = 0; i < params->polyCount; ++i) {
            polyFlags[i] = 0x01; // Default walkable flag
        }
    }
};

// Custom deleters
void NavTileCache::TileCacheDeleter::operator()(dtTileCache* tc) const {
    if (tc) {
        dtFreeTileCache(tc);
    }
}

void NavTileCache::AllocDeleter::operator()(dtTileCacheAlloc* alloc) const {
    delete alloc;
}

void NavTileCache::CompressorDeleter::operator()(dtTileCacheCompressor* comp) const {
    delete comp;
}

void NavTileCache::MeshProcessDeleter::operator()(dtTileCacheMeshProcess* proc) const {
    delete proc;
}

NavTileCache::NavTileCache() = default;

NavTileCache::~NavTileCache() {
    shutdown();
}

bool NavTileCache::init(NavMesh* navmesh, const NavTileCacheSettings& settings) {
    if (!navmesh || !navmesh->is_valid()) {
        core::log(core::LogLevel::Error, "NavTileCache: Invalid navmesh");
        return false;
    }

    if (!navmesh->supports_tile_cache()) {
        core::log(core::LogLevel::Error, "NavTileCache: Navmesh does not support tile cache (must be built with build_tiled)");
        return false;
    }

    m_navmesh = navmesh;
    m_settings = settings;

    // Create allocator
    m_alloc.reset(new LinearAllocator(32000));

    // Create compressor
    m_compressor.reset(new FastLZCompressor());

    // Create mesh processor
    m_mesh_process.reset(new MeshProcess());

    // Create tile cache
    dtTileCache* tc = dtAllocTileCache();
    if (!tc) {
        core::log(core::LogLevel::Error, "NavTileCache: Failed to allocate tile cache");
        return false;
    }

    // Initialize tile cache params
    dtTileCacheParams tcparams;
    std::memset(&tcparams, 0, sizeof(tcparams));

    const dtNavMesh* nm = navmesh->get_detour_navmesh();
    const dtNavMeshParams* nmparams = nm->getParams();

    // Copy navmesh origin and tile dimensions
    dtVcopy(tcparams.orig, nmparams->orig);
    tcparams.cs = 0.3f;  // Cell size - should match navmesh settings
    tcparams.ch = 0.2f;  // Cell height - should match navmesh settings
    tcparams.width = static_cast<int>(nmparams->tileWidth);
    tcparams.height = static_cast<int>(nmparams->tileHeight);
    tcparams.walkableHeight = 2.0f;
    tcparams.walkableRadius = 0.6f;
    tcparams.walkableClimb = 0.9f;
    tcparams.maxSimplificationError = settings.max_simplification_error;
    tcparams.maxTiles = nmparams->maxTiles;
    tcparams.maxObstacles = settings.max_obstacles;

    dtStatus status = tc->init(&tcparams, m_alloc.get(), m_compressor.get(), m_mesh_process.get());
    if (dtStatusFailed(status)) {
        dtFreeTileCache(tc);
        core::log(core::LogLevel::Error, "NavTileCache: Failed to initialize tile cache");
        return false;
    }

    m_tile_cache.reset(tc);

    // Load tile cache layers from navmesh
    const auto& layers = navmesh->get_tile_cache_layers();
    for (const auto& layer_data : layers) {
        if (!layer_data.empty()) {
            dtCompressedTileRef ref;
            // DetourTileCache expects non-const pointer but copies data
            status = tc->addTile(const_cast<unsigned char*>(layer_data.data()), static_cast<int>(layer_data.size()),
                                 DT_COMPRESSEDTILE_FREE_DATA, &ref);
            if (dtStatusFailed(status)) {
                core::log(core::LogLevel::Warn, "NavTileCache: Failed to add tile layer");
            }
        }
    }

    // Build initial navmesh from tiles
    for (int i = 0; i < tc->getTileCount(); ++i) {
        const dtCompressedTile* tile = tc->getTile(i);
        if (tile && tile->header) {
            tc->buildNavMeshTile(tc->getTileRef(tile), navmesh->get_detour_navmesh());
        }
    }

    core::log(core::LogLevel::Info, "NavTileCache initialized (max {} obstacles)", settings.max_obstacles);
    return true;
}

void NavTileCache::shutdown() {
    m_tile_cache.reset();
    m_mesh_process.reset();
    m_compressor.reset();
    m_alloc.reset();
    m_navmesh = nullptr;
    m_active_obstacles = 0;
}

ObstacleResult NavTileCache::add_cylinder(const Vec3& position, float radius, float height) {
    ObstacleResult result;

    if (!m_tile_cache) {
        result.error_message = "Tile cache not initialized";
        return result;
    }

    dtObstacleRef ref;
    dtStatus status = m_tile_cache->addObstacle(&position[0], radius, height, &ref);

    if (dtStatusFailed(status)) {
        result.error_message = "Failed to add cylinder obstacle";
        return result;
    }

    result.handle.id = static_cast<uint32_t>(ref);
    result.success = true;
    m_active_obstacles++;
    return result;
}

ObstacleResult NavTileCache::add_box(const Vec3& center, const Vec3& half_extents) {
    ObstacleResult result;

    if (!m_tile_cache) {
        result.error_message = "Tile cache not initialized";
        return result;
    }

    dtObstacleRef ref;
    dtStatus status = m_tile_cache->addBoxObstacle(&center[0], &half_extents[0], 0.0f, &ref);

    if (dtStatusFailed(status)) {
        result.error_message = "Failed to add box obstacle";
        return result;
    }

    result.handle.id = static_cast<uint32_t>(ref);
    result.success = true;
    m_active_obstacles++;
    return result;
}

ObstacleResult NavTileCache::add_oriented_box(const Vec3& center, const Vec3& half_extents, float y_rotation_radians) {
    ObstacleResult result;

    if (!m_tile_cache) {
        result.error_message = "Tile cache not initialized";
        return result;
    }

    dtObstacleRef ref;
    dtStatus status = m_tile_cache->addBoxObstacle(&center[0], &half_extents[0], y_rotation_radians, &ref);

    if (dtStatusFailed(status)) {
        result.error_message = "Failed to add oriented box obstacle";
        return result;
    }

    result.handle.id = static_cast<uint32_t>(ref);
    result.success = true;
    m_active_obstacles++;
    return result;
}

void NavTileCache::remove_obstacle(NavObstacleHandle handle) {
    if (!m_tile_cache || !handle.valid()) return;

    dtStatus status = m_tile_cache->removeObstacle(static_cast<dtObstacleRef>(handle.id));
    if (dtStatusSucceed(status)) {
        m_active_obstacles--;
    }
}

ObstacleResult NavTileCache::update_cylinder(NavObstacleHandle& handle, const Vec3& position, float radius, float height) {
    if (handle.valid()) {
        remove_obstacle(handle);
    }
    ObstacleResult result = add_cylinder(position, radius, height);
    if (result.success) {
        handle = result.handle;
    }
    return result;
}

ObstacleResult NavTileCache::update_box(NavObstacleHandle& handle, const Vec3& center, const Vec3& half_extents) {
    if (handle.valid()) {
        remove_obstacle(handle);
    }
    ObstacleResult result = add_box(center, half_extents);
    if (result.success) {
        handle = result.handle;
    }
    return result;
}

ObstacleResult NavTileCache::update_oriented_box(NavObstacleHandle& handle, const Vec3& center, const Vec3& half_extents, float y_rotation_radians) {
    if (handle.valid()) {
        remove_obstacle(handle);
    }
    ObstacleResult result = add_oriented_box(center, half_extents, y_rotation_radians);
    if (result.success) {
        handle = result.handle;
    }
    return result;
}

bool NavTileCache::update(float dt) {
    if (!m_tile_cache || !m_navmesh) return true;

    bool upToDate = false;
    m_tile_cache->update(dt, m_navmesh->get_detour_navmesh(), &upToDate);
    return upToDate;
}

} // namespace engine::navigation
