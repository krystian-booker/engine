#pragma once

// Umbrella header for terrain module
#include <engine/terrain/heightmap.hpp>
#include <engine/terrain/terrain_lod.hpp>
#include <engine/terrain/terrain_renderer.hpp>

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <engine/core/math.hpp>

namespace engine::terrain {

using namespace engine::core;

// Terrain configuration
struct TerrainConfig {
    // Dimensions
    Vec3 position = Vec3(0.0f);             // World position (corner)
    Vec3 scale = Vec3(512.0f, 100.0f, 512.0f);  // World size (X, Height, Z)

    // Data
    std::string heightmap_path;
    std::string splat_map_path;
    std::string hole_map_path;

    // Rendering
    TerrainRenderSettings render_settings;

    // Physics
    bool generate_collision = true;
    uint32_t collision_resolution = 0;       // 0 = auto (based on render resolution)

    // Streaming
    bool enable_streaming = false;
    float streaming_distance = 500.0f;
};

// Terrain modification operation
struct TerrainBrush {
    enum class Mode {
        Raise,      // Raise height
        Lower,      // Lower height
        Flatten,    // Flatten to target height
        Smooth,     // Smooth terrain
        Noise,      // Add noise
        Paint       // Paint splat map
    };

    Mode mode = Mode::Raise;
    float radius = 10.0f;
    float strength = 1.0f;
    float falloff = 0.5f;
    float target_height = 0.0f;              // For Flatten mode
    uint32_t paint_channel = 0;              // For Paint mode
};

// Main Terrain class
class Terrain {
public:
    Terrain() = default;
    ~Terrain() = default;

    // Non-copyable
    Terrain(const Terrain&) = delete;
    Terrain& operator=(const Terrain&) = delete;

    // Moveable
    Terrain(Terrain&&) = default;
    Terrain& operator=(Terrain&&) = default;

    // Creation
    bool create(const TerrainConfig& config);
    bool create_flat(const Vec3& position, const Vec3& scale, uint32_t resolution = 513);
    bool create_from_heightmap(const std::string& path, const Vec3& position, const Vec3& scale);
    void destroy();
    bool is_valid() const { return m_initialized; }

    // Configuration
    const TerrainConfig& get_config() const { return m_config; }
    void set_position(const Vec3& position);
    Vec3 get_position() const { return m_config.position; }
    Vec3 get_scale() const { return m_config.scale; }
    AABB get_bounds() const;

    // Height queries
    float get_height_at(float world_x, float world_z) const;
    Vec3 get_normal_at(float world_x, float world_z) const;
    bool get_height_and_normal(float world_x, float world_z, float& out_height, Vec3& out_normal) const;

    // Raycasting
    bool raycast(const Vec3& origin, const Vec3& direction, float max_dist,
                 Vec3& out_hit, Vec3& out_normal) const;

    // Point queries
    bool is_point_on_terrain(float world_x, float world_z) const;
    Vec3 project_point_to_terrain(const Vec3& point) const;

    // Layers
    void set_layer(uint32_t index, const TerrainLayer& layer);
    const TerrainLayer& get_layer(uint32_t index) const;

    // Heightmap access
    Heightmap& get_heightmap() { return m_heightmap; }
    const Heightmap& get_heightmap() const { return m_heightmap; }

    // Splat map access
    SplatMap& get_splat_map() { return m_splat_map; }
    const SplatMap& get_splat_map() const { return m_splat_map; }

    // Hole map access
    HoleMap& get_hole_map() { return m_hole_map; }
    const HoleMap& get_hole_map() const { return m_hole_map; }

    // Renderer access
    TerrainRenderer& get_renderer() { return m_renderer; }
    const TerrainRenderer& get_renderer() const { return m_renderer; }

    // Modification (editor)
    void apply_brush(const Vec3& world_pos, const TerrainBrush& brush, float dt);
    void mark_dirty(const AABB& region);
    void rebuild_dirty_chunks();

    // Update
    void update(float dt, const Vec3& camera_position, const Frustum& frustum);

    // Rendering
    void render(uint16_t view_id);
    void render_shadow(uint16_t view_id);

    // Physics (returns collision body handle if physics integration enabled)
    uint32_t get_physics_body() const { return m_physics_body; }
    void rebuild_collision();

    // Serialization
    bool save_to_file(const std::string& directory) const;
    bool load_from_file(const std::string& directory);

private:
    void update_collision();
    Vec2 world_to_uv(float world_x, float world_z) const;
    Vec3 uv_to_world(float u, float v, float height) const;

    TerrainConfig m_config;
    bool m_initialized = false;

    Heightmap m_heightmap;
    SplatMap m_splat_map;
    HoleMap m_hole_map;
    TerrainRenderer m_renderer;

    // Physics
    uint32_t m_physics_body = UINT32_MAX;
    std::vector<Vec3> m_collision_vertices;
    std::vector<uint32_t> m_collision_indices;

    // Dirty tracking for modifications
    std::vector<AABB> m_dirty_regions;
    bool m_collision_dirty = false;
};

// Global terrain manager
class TerrainManager {
public:
    TerrainManager() = default;
    ~TerrainManager() = default;

    // Singleton access
    static TerrainManager& instance();

    // Terrain management
    uint32_t create_terrain(const TerrainConfig& config);
    void destroy_terrain(uint32_t id);
    void destroy_all();

    Terrain* get_terrain(uint32_t id);
    const Terrain* get_terrain(uint32_t id) const;

    // Query across all terrains
    float get_height_at(float world_x, float world_z) const;
    Vec3 get_normal_at(float world_x, float world_z) const;
    bool raycast(const Vec3& origin, const Vec3& direction, float max_dist,
                 Vec3& out_hit, Vec3& out_normal, uint32_t* out_terrain_id = nullptr) const;
    Terrain* get_terrain_at(float world_x, float world_z);

    // Update all terrains
    void update(float dt, const Vec3& camera_position, const Frustum& frustum);

    // Render all terrains
    void render(uint16_t view_id);
    void render_shadows(uint16_t view_id);

    // Iteration
    std::vector<uint32_t> get_all_terrain_ids() const;
    void for_each_terrain(std::function<void(Terrain&)> func);

private:
    std::unordered_map<uint32_t, std::unique_ptr<Terrain>> m_terrains;
    uint32_t m_next_id = 1;
};

// Global function
TerrainManager& get_terrain_manager();

// ECS component for terrain entities
struct TerrainComponent {
    uint32_t terrain_id = UINT32_MAX;

    // Runtime cache
    Terrain* terrain_ptr = nullptr;
};

// Helper to create terrain from component
inline Terrain* get_terrain_from_component(const TerrainComponent& comp) {
    if (comp.terrain_ptr) return comp.terrain_ptr;
    return get_terrain_manager().get_terrain(comp.terrain_id);
}

} // namespace engine::terrain
