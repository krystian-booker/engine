#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <engine/core/math.hpp>

namespace engine::scene { class World; }

namespace engine::streaming {

using namespace engine::core;

// Volume shape types
enum class VolumeShape : uint8_t {
    Box,
    Sphere,
    Capsule,
    Cylinder
};

// Streaming volume - defines areas that control streaming behavior
struct StreamingVolume {
    std::string name;
    VolumeShape shape = VolumeShape::Box;

    // Transform
    Vec3 position;
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    Vec3 scale = Vec3(1.0f);

    // Shape parameters
    Vec3 box_extents = Vec3(10.0f);      // Half-extents for box
    float sphere_radius = 10.0f;          // Radius for sphere
    float capsule_radius = 5.0f;          // Radius for capsule
    float capsule_height = 10.0f;         // Height for capsule
    float cylinder_radius = 5.0f;         // Radius for cylinder
    float cylinder_height = 10.0f;        // Height for cylinder

    // Streaming behavior
    std::vector<std::string> load_cells;      // Cells to load when inside
    std::vector<std::string> unload_cells;    // Cells to unload when inside
    std::vector<std::string> preload_cells;   // Cells to preload (not visible yet)

    // Transition settings
    float fade_distance = 5.0f;           // Distance over which to fade in/out
    bool block_until_loaded = false;      // Block player until cells are loaded
    float blocking_timeout = 10.0f;       // Max time to block

    // Activation
    bool enabled = true;
    bool one_shot = false;                // Only trigger once
    bool player_only = true;              // Only trigger for player
    uint32_t activation_layers = 0xFFFFFFFF;  // Layer mask

    // Runtime state
    bool is_active = false;
    bool was_inside = false;
    float current_fade = 0.0f;
};

// Volume events
enum class VolumeEvent : uint8_t {
    Enter,
    Exit,
    Stay
};

using VolumeCallback = std::function<void(const StreamingVolume& volume, VolumeEvent event)>;

// Streaming volume manager
class StreamingVolumeManager {
public:
    StreamingVolumeManager() = default;
    ~StreamingVolumeManager() = default;

    // Volume registration
    void add_volume(const StreamingVolume& volume);
    void remove_volume(const std::string& name);
    void clear_volumes();

    // Volume access
    StreamingVolume* get_volume(const std::string& name);
    const StreamingVolume* get_volume(const std::string& name) const;
    std::vector<std::string> get_all_volume_names() const;

    // Enable/disable volumes
    void set_volume_enabled(const std::string& name, bool enabled);
    bool is_volume_enabled(const std::string& name) const;

    // Update - checks positions against volumes
    void update(const Vec3& player_position, uint32_t player_layer = 0xFFFFFFFF);

    // Query which volumes contain a point
    std::vector<std::string> get_volumes_at_point(const Vec3& point) const;
    bool is_point_in_volume(const std::string& name, const Vec3& point) const;

    // Callbacks
    void set_volume_callback(VolumeCallback callback) { m_on_volume_event = callback; }

    // Get cells that should be loaded based on current position
    std::vector<std::string> get_cells_to_load() const;
    std::vector<std::string> get_cells_to_unload() const;
    std::vector<std::string> get_cells_to_preload() const;

    // Check if blocking is needed
    bool is_blocking_required() const;
    float get_blocking_progress() const;

    // Debug
    void debug_draw() const;

private:
    bool test_point_in_volume(const StreamingVolume& volume, const Vec3& point) const;
    float get_signed_distance(const StreamingVolume& volume, const Vec3& point) const;

    std::vector<StreamingVolume> m_volumes;
    VolumeCallback m_on_volume_event;

    // Current state
    std::vector<std::string> m_active_volumes;
    std::vector<std::string> m_pending_loads;
    std::vector<std::string> m_pending_unloads;
    std::vector<std::string> m_pending_preloads;
    bool m_blocking = false;
    float m_blocking_progress = 0.0f;
};

// Global streaming volume manager
StreamingVolumeManager& get_streaming_volumes();

// ECS component for streaming volume triggers
struct StreamingVolumeComponent {
    std::string volume_name;             // Reference to a StreamingVolume
    bool use_entity_bounds = false;      // Use entity bounds instead of volume shape

    // Inline volume definition (alternative to referencing by name)
    bool use_inline_volume = false;
    StreamingVolume inline_volume;
};

// Portal component - connects two streaming cells
struct StreamingPortalComponent {
    std::string cell_a;
    std::string cell_b;
    Vec3 position;
    Vec3 normal;                         // Which way the portal faces
    float width = 5.0f;
    float height = 3.0f;
    bool bidirectional = true;
    bool occlude = true;                 // Can be used for occlusion
};

// Helper to create common volume types
namespace StreamingVolumeFactory {

inline StreamingVolume create_box(const std::string& name, const Vec3& position,
                                   const Vec3& half_extents,
                                   const std::vector<std::string>& load_cells) {
    StreamingVolume vol;
    vol.name = name;
    vol.shape = VolumeShape::Box;
    vol.position = position;
    vol.box_extents = half_extents;
    vol.load_cells = load_cells;
    return vol;
}

inline StreamingVolume create_sphere(const std::string& name, const Vec3& position,
                                      float radius,
                                      const std::vector<std::string>& load_cells) {
    StreamingVolume vol;
    vol.name = name;
    vol.shape = VolumeShape::Sphere;
    vol.position = position;
    vol.sphere_radius = radius;
    vol.load_cells = load_cells;
    return vol;
}

inline StreamingVolume create_loading_zone(const std::string& name, const Vec3& position,
                                            const Vec3& half_extents,
                                            const std::vector<std::string>& load_cells,
                                            bool block = false) {
    StreamingVolume vol;
    vol.name = name;
    vol.shape = VolumeShape::Box;
    vol.position = position;
    vol.box_extents = half_extents;
    vol.load_cells = load_cells;
    vol.block_until_loaded = block;
    return vol;
}

inline StreamingVolume create_level_transition(const std::string& name, const Vec3& position,
                                                const Vec3& half_extents,
                                                const std::vector<std::string>& load_cells,
                                                const std::vector<std::string>& unload_cells) {
    StreamingVolume vol;
    vol.name = name;
    vol.shape = VolumeShape::Box;
    vol.position = position;
    vol.box_extents = half_extents;
    vol.load_cells = load_cells;
    vol.unload_cells = unload_cells;
    vol.block_until_loaded = true;
    return vol;
}

} // namespace StreamingVolumeFactory

// ============================================================================
// Portal connectivity graph
// ============================================================================

struct PortalGraph {
    struct PortalEdge {
        std::string target_cell;
        Vec3 portal_center;
        Vec3 portal_normal;
        float width;
        float height;
    };

    std::unordered_map<std::string, std::vector<PortalEdge>> adjacency;

    void add_portal(const std::string& from_cell, const PortalEdge& edge);
    void clear();
    const std::vector<PortalEdge>* get_portals_from(const std::string& cell) const;
};

PortalGraph& get_portal_graph();

// ============================================================================
// ECS Systems
// ============================================================================

// Handles StreamingComponent: stream_with_player and persist_across_cells
// Should run FIRST to migrate entities before unload processing
void streaming_entity_system(engine::scene::World& world, double dt);

// Processes StreamingZoneComponent radius-based triggers
void streaming_zone_system(engine::scene::World& world, double dt);

// Syncs entity transforms to volumes, forwards load/unload to SceneStreamingSystem
void streaming_volume_system(engine::scene::World& world, double dt);

// Manages portal visibility and boosts loading priority for visible cells
void streaming_portal_system(engine::scene::World& world, double dt);

// Core streaming update - processes load/unload queues, should run LAST
void streaming_update_system(engine::scene::World& world, double dt);

} // namespace engine::streaming
