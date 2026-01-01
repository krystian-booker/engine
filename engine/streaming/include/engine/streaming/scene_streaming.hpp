#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <queue>
#include <mutex>
#include <future>
#include <engine/core/math.hpp>

namespace engine::streaming {

using namespace engine::core;

// Forward declarations
class StreamingCell;
class SceneStreamingSystem;

// Streaming cell state
enum class CellState : uint8_t {
    Unloaded,       // Not in memory
    Loading,        // Being loaded asynchronously
    Loaded,         // Fully loaded but not visible
    Visible,        // Loaded and visible
    Unloading       // Being unloaded
};

// Cell LOD level
enum class CellLOD : uint8_t {
    Full,           // Full detail
    Reduced,        // Reduced detail (distant)
    Proxy,          // Impostor/proxy only
    Hidden          // Not rendered (too far)
};

// Streaming priority
enum class StreamingPriority : uint8_t {
    Critical,       // Load immediately (player area)
    High,           // Load soon (adjacent to player)
    Normal,         // Standard loading
    Low,            // Load when idle
    Background      // Load only if nothing else queued
};

// Streaming cell - a chunk of the world that can be loaded/unloaded
struct StreamingCellData {
    std::string name;
    std::string scene_path;         // Path to scene file
    AABB bounds;                    // World-space bounds
    float load_distance = 100.0f;   // Distance to start loading
    float unload_distance = 150.0f; // Distance to unload
    std::vector<std::string> dependencies;  // Other cells that must be loaded first

    // Runtime data
    CellState state = CellState::Unloaded;
    CellLOD lod = CellLOD::Hidden;
    float distance_to_player = FLT_MAX;
    StreamingPriority priority = StreamingPriority::Normal;
    uint64_t last_visible_time = 0;
    std::vector<uint32_t> entity_ids;  // Entities belonging to this cell
};

// Streaming settings
struct StreamingSettings {
    // Distances
    float base_load_distance = 100.0f;
    float base_unload_distance = 150.0f;
    float lod_distance_multiplier = 1.5f;  // Multiplier between LOD levels

    // Performance
    uint32_t max_concurrent_loads = 2;
    uint32_t max_loads_per_frame = 1;
    uint32_t max_unloads_per_frame = 1;
    float load_budget_ms = 5.0f;           // Max time for loading per frame

    // Memory
    uint64_t max_loaded_memory = 512 * 1024 * 1024;  // 512 MB default
    bool aggressive_unload = false;         // Unload when memory is tight

    // LOD
    bool use_lod = true;
    float lod_bias = 0.0f;                  // Positive = higher quality

    // Streaming source position
    bool use_camera_position = true;        // Use camera instead of player
    Vec3 override_position;                 // Manual override position
};

// Streaming statistics
struct StreamingStats {
    uint32_t total_cells = 0;
    uint32_t loaded_cells = 0;
    uint32_t visible_cells = 0;
    uint32_t loading_cells = 0;
    uint32_t unloading_cells = 0;
    uint64_t loaded_memory = 0;
    uint32_t loads_this_frame = 0;
    uint32_t unloads_this_frame = 0;
    float average_load_time_ms = 0.0f;
};

// Streaming event callbacks
using CellLoadedCallback = std::function<void(const std::string& cell_name)>;
using CellUnloadedCallback = std::function<void(const std::string& cell_name)>;
using CellVisibleCallback = std::function<void(const std::string& cell_name, bool visible)>;

// Load request for async loading
struct StreamingLoadRequest {
    std::string cell_name;
    StreamingPriority priority = StreamingPriority::Normal;
    float distance = FLT_MAX;

    bool operator<(const StreamingLoadRequest& other) const {
        // Higher priority and closer distance come first
        if (priority != other.priority) {
            return static_cast<int>(priority) > static_cast<int>(other.priority);
        }
        return distance > other.distance;
    }
};

// Scene streaming system
class SceneStreamingSystem {
public:
    SceneStreamingSystem() = default;
    ~SceneStreamingSystem() = default;

    // Non-copyable
    SceneStreamingSystem(const SceneStreamingSystem&) = delete;
    SceneStreamingSystem& operator=(const SceneStreamingSystem&) = delete;

    // Initialize/shutdown
    void init(const StreamingSettings& settings = {});
    void shutdown();
    bool is_initialized() const { return m_initialized; }

    // Configuration
    void set_settings(const StreamingSettings& settings) { m_settings = settings; }
    const StreamingSettings& get_settings() const { return m_settings; }

    // Cell registration
    void register_cell(const StreamingCellData& cell);
    void unregister_cell(const std::string& name);
    void clear_cells();

    // Cell access
    StreamingCellData* get_cell(const std::string& name);
    const StreamingCellData* get_cell(const std::string& name) const;
    std::vector<std::string> get_all_cell_names() const;
    std::vector<std::string> get_loaded_cell_names() const;
    std::vector<std::string> get_visible_cell_names() const;

    // Manual loading control
    void request_load(const std::string& cell_name, StreamingPriority priority = StreamingPriority::Normal);
    void request_unload(const std::string& cell_name);
    void force_load_sync(const std::string& cell_name);  // Blocking load
    void force_unload_sync(const std::string& cell_name); // Blocking unload

    // Batch operations
    void load_cells_in_radius(const Vec3& center, float radius);
    void unload_cells_outside_radius(const Vec3& center, float radius);
    void preload_cells(const std::vector<std::string>& cell_names);

    // Update (call each frame)
    void update(float dt, const Vec3& player_position, const Vec3& camera_position);

    // Set streaming origin (alternative to update parameters)
    void set_streaming_origin(const Vec3& position);

    // Callbacks
    void set_load_callback(CellLoadedCallback callback) { m_on_loaded = callback; }
    void set_unload_callback(CellUnloadedCallback callback) { m_on_unloaded = callback; }
    void set_visibility_callback(CellVisibleCallback callback) { m_on_visibility_changed = callback; }

    // Custom loader (for integration with asset system)
    using CellLoader = std::function<bool(const std::string& scene_path, std::vector<uint32_t>& out_entities)>;
    using CellUnloader = std::function<void(const std::vector<uint32_t>& entities)>;
    void set_cell_loader(CellLoader loader) { m_cell_loader = loader; }
    void set_cell_unloader(CellUnloader unloader) { m_cell_unloader = unloader; }

    // Statistics
    StreamingStats get_stats() const;

    // Debug
    void debug_draw() const;
    bool is_cell_loaded(const std::string& name) const;
    bool is_cell_visible(const std::string& name) const;
    CellState get_cell_state(const std::string& name) const;

private:
    void update_cell_distances(const Vec3& origin);
    void update_cell_priorities();
    void update_cell_lods();
    void process_load_queue();
    void process_unload_queue();
    void check_async_loads();

    bool load_cell_internal(StreamingCellData& cell);
    void unload_cell_internal(StreamingCellData& cell);
    void update_cell_visibility(StreamingCellData& cell, const Vec3& origin);

    StreamingSettings m_settings;
    bool m_initialized = false;

    std::unordered_map<std::string, StreamingCellData> m_cells;
    std::priority_queue<StreamingLoadRequest> m_load_queue;
    std::vector<std::string> m_unload_queue;

    // Async loading
    struct AsyncLoadTask {
        std::string cell_name;
        std::future<bool> future;
        std::vector<uint32_t> loaded_entities;
    };
    std::vector<AsyncLoadTask> m_async_loads;
    std::mutex m_async_mutex;

    // Callbacks
    CellLoadedCallback m_on_loaded;
    CellUnloadedCallback m_on_unloaded;
    CellVisibleCallback m_on_visibility_changed;

    // Custom loader/unloader
    CellLoader m_cell_loader;
    CellUnloader m_cell_unloader;

    // Stats tracking
    mutable StreamingStats m_stats;
    std::vector<float> m_load_times;

    Vec3 m_streaming_origin;
    uint64_t m_current_time = 0;
};

// Global scene streaming system
SceneStreamingSystem& get_scene_streaming();

// ECS component for streaming-aware entities
struct StreamingComponent {
    std::string cell_name;           // Which cell this entity belongs to
    bool persist_across_cells = false;  // Keep loaded when cell unloads
    bool stream_with_player = false;    // Always stay near player
};

// Streaming zone component (marks areas that trigger streaming)
struct StreamingZoneComponent {
    std::vector<std::string> cells_to_load;     // Cells to load when entering
    std::vector<std::string> cells_to_unload;   // Cells to unload when entering
    float activation_radius = 10.0f;
    bool one_shot = false;                       // Only trigger once
    bool triggered = false;
};

} // namespace engine::streaming
