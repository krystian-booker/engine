#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <engine/core/math.hpp>
#include <engine/vegetation/grass.hpp>

namespace engine::vegetation {

using namespace engine::core;

// Foliage LOD level
struct FoliageLOD {
    uint32_t mesh_id = UINT32_MAX;          // Mesh handle
    uint32_t material_id = UINT32_MAX;      // Material handle
    float screen_size = 0.0f;               // Min screen size for this LOD
    float transition_width = 0.1f;          // Dithering transition width
};

// Billboard settings for distant foliage
struct FoliageBillboard {
    uint32_t texture = UINT32_MAX;          // Billboard atlas texture
    Vec2 size = Vec2(4.0f, 6.0f);           // Billboard size
    Vec2 uv_min = Vec2(0.0f);               // UV coordinates in atlas
    Vec2 uv_max = Vec2(1.0f);
    bool rotate_to_camera = true;           // Always face camera
    float start_distance = 100.0f;          // Distance to switch to billboard
};

// Foliage type definition (e.g., oak tree, pine tree, bush)
struct FoliageType {
    std::string name;
    std::string id;

    // LOD meshes
    std::vector<FoliageLOD> lods;

    // Billboard (optional, for very far distances)
    bool use_billboard = true;
    FoliageBillboard billboard;

    // Scale variation
    float min_scale = 0.8f;
    float max_scale = 1.2f;

    // Rotation
    bool random_rotation = true;
    float min_rotation = 0.0f;
    float max_rotation = 360.0f;

    // Alignment
    bool align_to_terrain = true;
    float max_slope = 45.0f;                // Max slope in degrees
    float terrain_offset = 0.0f;            // Vertical offset from terrain

    // Collision
    bool has_collision = true;
    float collision_radius = 0.5f;
    float collision_height = 5.0f;

    // Wind
    bool affected_by_wind = true;
    float wind_strength = 0.3f;
    float wind_frequency = 1.0f;

    // Shadows
    bool cast_shadows = true;
    bool receive_shadows = true;

    // Culling
    float cull_distance = 500.0f;
    float fade_distance = 50.0f;
};

// Foliage instance data
struct FoliageInstance {
    Vec3 position;
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    float scale = 1.0f;
    uint32_t type_index = 0;
    uint32_t random_seed = 0;

    // Runtime state
    uint32_t current_lod = 0;
    float lod_blend = 0.0f;
    bool visible = true;
    bool use_billboard = false;
};

// Foliage placement rules
struct FoliagePlacementRule {
    std::string type_id;
    float density = 0.1f;                   // Instances per square unit

    // Height constraints
    float min_height = 0.0f;
    float max_height = 1000.0f;

    // Slope constraints
    float min_slope = 0.0f;
    float max_slope = 30.0f;

    // Noise-based distribution
    float noise_scale = 10.0f;
    float noise_threshold = 0.3f;

    // Clustering
    bool enable_clustering = true;
    float cluster_radius = 5.0f;
    uint32_t cluster_count = 3;

    // Exclusion zones
    std::vector<AABB> exclusion_zones;

    // Custom filter
    std::function<bool(const Vec3& position, const Vec3& normal)> custom_filter;
};

// Foliage chunk
struct FoliageChunk {
    AABB bounds;
    std::vector<uint32_t> instance_indices;  // Indices into instance array
    bool visible = false;
    float distance_to_camera = 0.0f;
};

// Foliage rendering settings
struct FoliageSettings {
    // Quality
    uint32_t max_instances = 50000;
    float lod_bias = 0.0f;
    bool use_gpu_culling = true;

    // Billboards
    bool enable_billboards = true;
    float billboard_start_distance = 100.0f;

    // Shadows
    bool cast_shadows = true;
    uint32_t shadow_lod = 1;                // Use this LOD for shadows

    // Wind
    bool enable_wind = true;
    Vec2 wind_direction = Vec2(1.0f, 0.0f);
    float wind_speed = 1.0f;
    float wind_strength = 0.3f;

    // Performance
    uint32_t chunk_size = 32;
    float update_distance = 20.0f;          // Distance change to trigger LOD update
};

// Foliage system
class FoliageSystem {
public:
    FoliageSystem() = default;
    ~FoliageSystem();

    // Initialize
    void init(const AABB& bounds, const FoliageSettings& settings = {});
    void shutdown();
    bool is_initialized() const { return m_initialized; }

    // Settings
    void set_settings(const FoliageSettings& settings);
    const FoliageSettings& get_settings() const { return m_settings; }

    // Type registration
    void register_type(const FoliageType& type);
    void unregister_type(const std::string& id);
    const FoliageType* get_type(const std::string& id) const;
    std::vector<std::string> get_all_type_ids() const;

    // Instance management
    uint32_t add_instance(const std::string& type_id, const Vec3& position,
                          const Quat& rotation = Quat{1.0f, 0.0f, 0.0f, 0.0f}, float scale = 1.0f);
    void remove_instance(uint32_t index);
    void clear_instances();

    // Bulk operations
    void add_instances(const std::string& type_id, const std::vector<Vec3>& positions);

    // Procedural placement
    void generate_from_rules(const std::vector<FoliagePlacementRule>& rules,
                             std::function<float(float x, float z)> height_func,
                             std::function<Vec3(float x, float z)> normal_func);

    void generate_in_region(const AABB& region, const FoliagePlacementRule& rule,
                            std::function<float(float x, float z)> height_func,
                            std::function<Vec3(float x, float z)> normal_func);

    // Update
    void update(float dt, const Vec3& camera_position, const Frustum& frustum);

    // Rendering
    void render(uint16_t view_id);
    void render_shadows(uint16_t view_id);

    // Query
    const FoliageInstance* get_instance(uint32_t index) const;
    std::vector<uint32_t> get_instances_in_radius(const Vec3& center, float radius) const;
    std::vector<uint32_t> get_instances_in_bounds(const AABB& bounds) const;

    // Raycasting
    bool raycast(const Vec3& origin, const Vec3& direction, float max_dist,
                 Vec3& out_hit, uint32_t& out_instance) const;

    // Statistics
    struct Stats {
        uint32_t total_instances = 0;
        uint32_t visible_instances = 0;
        uint32_t billboard_instances = 0;
        uint32_t total_types = 0;
        uint32_t visible_chunks = 0;
    };
    Stats get_stats() const { return m_stats; }

    // Serialization
    bool save_to_file(const std::string& path) const;
    bool load_from_file(const std::string& path);

private:
    void rebuild_chunks();
    void update_lods(const Vec3& camera_position);
    void update_visibility(const Vec3& camera_position, const Frustum& frustum);
    void update_wind(float dt);

    void render_instances(uint16_t view_id, bool shadow_pass);
    void render_billboards(uint16_t view_id);

    FoliageSettings m_settings;
    AABB m_bounds;
    bool m_initialized = false;

    // Types
    std::unordered_map<std::string, FoliageType> m_types;
    std::vector<std::string> m_type_order;  // For index lookup

    // Instances
    std::vector<FoliageInstance> m_instances;
    std::vector<FoliageChunk> m_chunks;

    // Wind state
    float m_wind_time = 0.0f;

    // GPU resources
    uint32_t m_instance_buffer = UINT32_MAX;
    uint32_t m_billboard_buffer = UINT32_MAX;
    uint32_t m_billboard_shader = UINT32_MAX;

    // Cached camera position for LOD updates
    Vec3 m_last_camera_pos;

    Stats m_stats;
};

// Global foliage system
FoliageSystem& get_foliage_system();

// ECS component
struct FoliageComponent {
    std::string type_id;
    float scale = 1.0f;
    bool cast_shadows = true;
    uint32_t instance_index = UINT32_MAX;   // Runtime index
};

// Vegetation manager - combines grass and foliage
class VegetationManager {
public:
    VegetationManager() = default;
    ~VegetationManager() = default;

    static VegetationManager& instance();

    // Initialize for a terrain
    void init(const AABB& terrain_bounds);
    void shutdown();

    // Access subsystems
    GrassSystem& grass() { return m_grass; }
    FoliageSystem& foliage() { return m_foliage; }

    // Update all vegetation
    void update(float dt, const Vec3& camera_position, const Frustum& frustum);

    // Render all vegetation
    void render(uint16_t view_id);
    void render_shadows(uint16_t view_id);

    // Generation from terrain data
    void generate_vegetation(
        std::function<float(float x, float z)> height_func,
        std::function<Vec3(float x, float z)> normal_func,
        std::function<float(float x, float z)> grass_density_func = nullptr,
        const std::vector<FoliagePlacementRule>& foliage_rules = {}
    );

    // Clear all vegetation
    void clear();

private:
    GrassSystem m_grass;
    FoliageSystem m_foliage;
    AABB m_bounds;
    bool m_initialized = false;
};

VegetationManager& get_vegetation_manager();

} // namespace engine::vegetation
