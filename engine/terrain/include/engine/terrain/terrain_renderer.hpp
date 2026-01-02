#pragma once

#include <engine/terrain/heightmap.hpp>
#include <engine/terrain/terrain_lod.hpp>
#include <engine/core/math.hpp>
#include <array>
#include <vector>
#include <memory>
#include <string>
#include <algorithm>

namespace engine::terrain {

using namespace engine::core;

// Maximum texture layers for terrain
constexpr uint32_t MAX_TERRAIN_LAYERS = 8;

// Terrain layer - defines textures for one material layer
struct TerrainLayer {
    std::string name;

    // Textures (handles)
    uint32_t albedo_texture = UINT32_MAX;       // Base color
    uint32_t normal_texture = UINT32_MAX;       // Normal map
    uint32_t arm_texture = UINT32_MAX;          // AO/Roughness/Metallic packed

    // Tiling
    float uv_scale = 10.0f;                     // Texture repeat per terrain unit
    Vec2 uv_offset = Vec2(0.0f);

    // PBR properties (used if no textures)
    Vec3 base_color = Vec3(0.5f);
    float roughness = 0.8f;
    float metallic = 0.0f;

    // Blending
    float height_blend = 0.5f;                  // Height-based blending strength
    float slope_blend = 0.0f;                   // Slope-based blending strength
};

// Terrain rendering settings
struct TerrainRenderSettings {
    // Quality
    uint32_t chunk_resolution = 65;             // Vertices per chunk edge (2^n + 1)
    uint32_t chunks_per_side = 16;              // Number of chunks per terrain side

    // LOD
    TerrainLODSettings lod_settings;

    // Rendering
    bool enable_tessellation = false;
    float tessellation_factor = 4.0f;
    float tessellation_max_distance = 100.0f;

    bool enable_parallax = true;
    float parallax_scale = 0.1f;

    bool enable_triplanar = true;               // Triplanar mapping on steep slopes
    float triplanar_sharpness = 8.0f;
    float triplanar_slope_threshold = 0.5f;

    // Detail textures
    bool enable_detail = true;
    float detail_distance = 50.0f;
    float detail_scale = 100.0f;

    // Shadows
    bool cast_shadows = true;
    bool receive_shadows = true;
};

// Terrain vertex data
struct TerrainVertex {
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
    Vec4 tangent;
};

// Terrain renderer - handles mesh generation and rendering
class TerrainRenderer {
public:
    TerrainRenderer() = default;
    ~TerrainRenderer();

    // Initialize with heightmap data
    void init(const Heightmap& heightmap, const Vec3& terrain_scale,
              const TerrainRenderSettings& settings = {});
    void shutdown();
    bool is_initialized() const { return m_initialized; }

    // Settings
    void set_settings(const TerrainRenderSettings& settings);
    const TerrainRenderSettings& get_settings() const { return m_settings; }

    // Layers
    void set_layer(uint32_t index, const TerrainLayer& layer);
    const TerrainLayer& get_layer(uint32_t index) const;
    void set_splat_map(const SplatMap& splat_map);

    // Update
    void update(const Vec3& camera_position, const Frustum& frustum);

    // Rendering
    void render(uint16_t view_id);
    void render_shadow(uint16_t view_id);

    // Mesh access (for physics)
    const std::vector<TerrainVertex>& get_vertices() const { return m_vertices; }
    const std::vector<uint32_t>& get_indices() const { return m_indices; }

    // Height queries
    float get_height_at(float world_x, float world_z) const;
    Vec3 get_normal_at(float world_x, float world_z) const;
    bool raycast(const Vec3& origin, const Vec3& direction, float max_dist,
                 Vec3& out_hit, Vec3& out_normal) const;

    // Chunk access
    uint32_t get_visible_chunk_count() const { return m_visible_chunk_count; }
    const std::vector<TerrainChunk>& get_chunks() const { return m_chunks; }

    // Debug
    void set_wireframe(bool enable) { m_wireframe = enable; }
    void set_show_chunks(bool enable) { m_show_chunks = enable; }

private:
    void generate_mesh();
    void generate_chunk(uint32_t chunk_x, uint32_t chunk_z);
    void update_chunk_lods(const Vec3& camera_pos);
    void update_visibility(const Frustum& frustum);
    void rebuild_index_buffer();

    void create_gpu_resources();
    void destroy_gpu_resources();
    void upload_chunk_data(const TerrainChunk& chunk);

    TerrainRenderSettings m_settings;
    bool m_initialized = false;

    // Terrain data
    const Heightmap* m_heightmap = nullptr;
    Vec3 m_terrain_scale = Vec3(1.0f);
    AABB m_terrain_bounds;

    // Layers
    std::array<TerrainLayer, MAX_TERRAIN_LAYERS> m_layers;
    uint32_t m_active_layer_count = 0;
    const SplatMap* m_splat_map = nullptr;

    // Mesh data
    std::vector<TerrainVertex> m_vertices;
    std::vector<uint32_t> m_indices;

    // Chunks
    std::vector<TerrainChunk> m_chunks;
    uint32_t m_visible_chunk_count = 0;

    // LOD
    TerrainLODSelector m_lod_selector;
    std::vector<std::vector<uint32_t>> m_lod_index_buffers;

    // GPU resources (bgfx handles would go here)
    uint32_t m_vertex_buffer = UINT32_MAX;
    uint32_t m_index_buffer = UINT32_MAX;
    uint32_t m_heightmap_texture = UINT32_MAX;
    uint32_t m_splat_texture = UINT32_MAX;
    uint32_t m_shader_program = UINT32_MAX;
    uint32_t m_shadow_program = UINT32_MAX;

    // Uniforms
    uint32_t m_u_terrain_scale = UINT32_MAX;
    uint32_t m_u_terrain_offset = UINT32_MAX;
    uint32_t m_u_chunk_offset = UINT32_MAX;
    uint32_t m_u_lod_params = UINT32_MAX;

    // Debug
    bool m_wireframe = false;
    bool m_show_chunks = false;
};

// Terrain physics mesh generator
class TerrainPhysicsGenerator {
public:
    // Generate collision mesh from heightmap
    static void generate_collision_mesh(const Heightmap& heightmap, const Vec3& terrain_scale,
                                         std::vector<Vec3>& out_vertices,
                                         std::vector<uint32_t>& out_indices,
                                         uint32_t resolution = 0);  // 0 = full resolution

    // Generate simplified collision mesh
    static void generate_simplified_mesh(const Heightmap& heightmap, const Vec3& terrain_scale,
                                          float simplification_ratio,
                                          std::vector<Vec3>& out_vertices,
                                          std::vector<uint32_t>& out_indices);

    // Generate height field data (for physics engines that support it)
    static void generate_height_field(const Heightmap& heightmap, const Vec3& terrain_scale,
                                       std::vector<float>& out_heights,
                                       uint32_t& out_rows, uint32_t& out_cols);
};

} // namespace engine::terrain
