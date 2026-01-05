#pragma once

#include <engine/terrain/heightmap.hpp>
#include <engine/terrain/terrain_lod.hpp>
#include <engine/core/math.hpp>
#include <vector>
#include <queue>
#include <mutex>
#include <future>
#include <unordered_map>
#include <memory>

namespace engine::terrain {

using namespace engine::core;

// Streaming chunk state
enum class StreamingChunkState : uint8_t {
    Unloaded,       // Not in memory
    Loading,        // Async loading in progress
    Loaded,         // In memory, GPU resources allocated
    Unloading       // Being unloaded
};

// Streaming chunk data
struct StreamingChunk {
    // Grid position (chunk coordinates)
    int32_t grid_x = 0;
    int32_t grid_z = 0;

    // World bounds
    AABB bounds;
    Vec3 center;

    // State
    StreamingChunkState state = StreamingChunkState::Unloaded;
    float distance_to_camera = std::numeric_limits<float>::max();
    float priority = 0.0f;

    // Heightmap region (subset of full heightmap or separate file)
    uint32_t heightmap_offset_x = 0;
    uint32_t heightmap_offset_z = 0;
    uint32_t heightmap_resolution = 65;

    // CPU data (loaded from disk)
    std::vector<float> height_data;

    // GPU handles (bgfx)
    uint32_t vertex_buffer = UINT32_MAX;
    uint32_t index_buffer = UINT32_MAX;

    // LOD state
    ChunkLOD lod;
    bool visible = false;
};

// Streaming load request
struct StreamingChunkRequest {
    int32_t grid_x;
    int32_t grid_z;
    float distance;
    float priority;

    bool operator<(const StreamingChunkRequest& other) const {
        return priority < other.priority;  // Higher priority first
    }
};

// Terrain streaming configuration
struct TerrainStreamingConfig {
    float load_distance = 500.0f;           // Distance to load chunks
    float unload_distance = 600.0f;         // Distance to unload chunks (hysteresis)

    uint32_t max_loaded_chunks = 64;        // Maximum chunks in memory
    uint32_t max_loads_per_frame = 2;       // Limit async loads per frame
    uint32_t max_unloads_per_frame = 2;     // Limit unloads per frame

    float load_budget_ms = 4.0f;            // Time budget for loading per frame

    // Chunk sizes
    uint32_t chunk_resolution = 65;         // Vertices per chunk edge
    float chunk_world_size = 64.0f;         // World units per chunk

    // Source data
    std::string heightmap_directory;        // Directory with chunk heightmap files
    bool use_single_heightmap = true;       // Use one large heightmap vs per-chunk files
};

// Terrain streamer - manages chunk loading/unloading
class TerrainStreamer {
public:
    TerrainStreamer() = default;
    ~TerrainStreamer();

    // Non-copyable
    TerrainStreamer(const TerrainStreamer&) = delete;
    TerrainStreamer& operator=(const TerrainStreamer&) = delete;

    // Initialization
    void init(const TerrainStreamingConfig& config, const AABB& terrain_bounds,
              const Heightmap* source_heightmap = nullptr);
    void shutdown();
    bool is_initialized() const { return m_initialized; }

    // Configuration
    void set_config(const TerrainStreamingConfig& config);
    const TerrainStreamingConfig& get_config() const { return m_config; }

    // Update (call each frame)
    void update(const Vec3& camera_position, const Frustum& frustum);

    // Get chunks for rendering
    void get_visible_chunks(std::vector<const StreamingChunk*>& out_chunks) const;
    void get_loaded_chunks(std::vector<const StreamingChunk*>& out_chunks) const;

    // Force load/unload
    void request_load(int32_t grid_x, int32_t grid_z);
    void request_unload(int32_t grid_x, int32_t grid_z);
    void force_load_sync(int32_t grid_x, int32_t grid_z);

    // Statistics
    uint32_t get_loaded_chunk_count() const { return m_loaded_count; }
    uint32_t get_visible_chunk_count() const { return m_visible_count; }
    uint32_t get_loading_chunk_count() const { return static_cast<uint32_t>(m_async_loads.size()); }
    float get_memory_usage_mb() const;

    // Access to chunks for rendering
    const std::unordered_map<uint64_t, StreamingChunk>& get_all_chunks() const {
        return m_chunks;
    }

private:
    // Grid coordinate to unique key
    static uint64_t make_chunk_key(int32_t x, int32_t z) {
        return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) |
               static_cast<uint32_t>(z);
    }

    void update_chunk_distances(const Vec3& camera_pos);
    void update_chunk_priorities();
    void update_visibility(const Frustum& frustum);
    void process_load_queue();
    void process_unload_queue();
    void check_async_loads();

    bool load_chunk_data(StreamingChunk& chunk);
    void upload_chunk_gpu(StreamingChunk& chunk);
    void unload_chunk(StreamingChunk& chunk);

    // Heightmap sampling for chunk region
    void sample_heightmap_region(StreamingChunk& chunk);

    TerrainStreamingConfig m_config;
    bool m_initialized = false;

    AABB m_terrain_bounds;
    int32_t m_grid_min_x = 0, m_grid_min_z = 0;
    int32_t m_grid_max_x = 0, m_grid_max_z = 0;

    // Source heightmap (if using single heightmap mode)
    const Heightmap* m_source_heightmap = nullptr;
    Vec3 m_terrain_scale;

    // All chunks (keyed by grid position)
    std::unordered_map<uint64_t, StreamingChunk> m_chunks;

    // Load/unload queues
    std::priority_queue<StreamingChunkRequest> m_load_queue;
    std::vector<uint64_t> m_unload_queue;

    // Async loading
    struct AsyncChunkLoad {
        uint64_t chunk_key;
        std::future<bool> future;
        std::vector<float> loaded_data;
    };
    std::vector<std::unique_ptr<AsyncChunkLoad>> m_async_loads;
    std::mutex m_async_mutex;

    // Statistics
    uint32_t m_loaded_count = 0;
    uint32_t m_visible_count = 0;
};

} // namespace engine::terrain
