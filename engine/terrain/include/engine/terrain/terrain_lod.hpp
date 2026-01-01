#pragma once

#include <vector>
#include <cstdint>
#include <engine/core/math.hpp>

namespace engine::terrain {

using namespace engine::core;

// Terrain LOD settings
struct TerrainLODSettings {
    uint32_t num_lods = 4;              // Number of LOD levels
    float lod_distance_ratio = 2.0f;    // Distance multiplier between LOD levels
    float base_lod_distance = 50.0f;    // Distance for LOD 0 -> LOD 1 transition
    float morph_range = 0.2f;           // Transition range (as ratio)
    bool use_geomorphing = true;        // Smooth LOD transitions
};

// Terrain chunk LOD data
struct ChunkLOD {
    uint32_t lod_level = 0;
    float morph_factor = 0.0f;          // 0 = this LOD, 1 = next LOD
    float distance_to_camera = 0.0f;

    // Edge LOD levels (for stitching with neighbors)
    uint32_t north_lod = 0;
    uint32_t south_lod = 0;
    uint32_t east_lod = 0;
    uint32_t west_lod = 0;

    bool needs_stitch = false;
};

// Terrain chunk - a piece of the terrain grid
struct TerrainChunk {
    // Grid position
    int32_t grid_x = 0;
    int32_t grid_z = 0;

    // World bounds
    AABB bounds;
    Vec3 center;

    // LOD state
    ChunkLOD lod;

    // Mesh data indices
    uint32_t mesh_id = UINT32_MAX;      // Index into mesh array
    uint32_t index_offset = 0;          // Offset into index buffer
    uint32_t index_count = 0;           // Number of indices for this chunk

    // Visibility
    bool visible = true;
    bool in_frustum = true;
};

// Terrain LOD selector - determines which LOD level to use
class TerrainLODSelector {
public:
    TerrainLODSelector() = default;

    void set_settings(const TerrainLODSettings& settings) { m_settings = settings; }
    const TerrainLODSettings& get_settings() const { return m_settings; }

    // Calculate LOD for a chunk based on camera
    ChunkLOD calculate_lod(const Vec3& chunk_center, const Vec3& camera_pos) const;

    // Get LOD level for a given distance
    uint32_t get_lod_for_distance(float distance) const;

    // Get transition distances
    float get_lod_start_distance(uint32_t lod) const;
    float get_lod_end_distance(uint32_t lod) const;

    // Calculate morph factor for smooth transitions
    float calculate_morph_factor(float distance, uint32_t lod) const;

private:
    TerrainLODSettings m_settings;
};

// Quadtree node for terrain LOD (alternative to uniform grid)
struct QuadtreeNode {
    AABB bounds;
    uint32_t depth = 0;
    uint32_t lod = 0;

    // Children (nullptr if leaf)
    std::unique_ptr<QuadtreeNode> children[4];  // NW, NE, SW, SE

    // Leaf data
    bool is_leaf = true;
    uint32_t chunk_index = UINT32_MAX;

    bool has_children() const { return !is_leaf && children[0] != nullptr; }
};

// Quadtree-based terrain LOD system
class TerrainQuadtree {
public:
    TerrainQuadtree() = default;

    void build(const AABB& terrain_bounds, uint32_t max_depth);
    void update(const Vec3& camera_pos, float lod_distance);

    // Get visible chunks at appropriate LOD levels
    void get_visible_chunks(const Frustum& frustum, std::vector<uint32_t>& out_chunks) const;

    // Get all leaf nodes
    void get_leaves(std::vector<const QuadtreeNode*>& out_leaves) const;

    const QuadtreeNode* get_root() const { return m_root.get(); }

private:
    void subdivide(QuadtreeNode* node, const Vec3& camera_pos, float lod_distance);
    void collect_visible(const QuadtreeNode* node, const Frustum& frustum,
                         std::vector<uint32_t>& out_chunks) const;
    void collect_leaves(const QuadtreeNode* node, std::vector<const QuadtreeNode*>& out_leaves) const;

    std::unique_ptr<QuadtreeNode> m_root;
    uint32_t m_max_depth = 5;
};

// Index buffer generator for terrain chunks
class TerrainIndexGenerator {
public:
    // Generate indices for a terrain grid
    static void generate_grid_indices(uint32_t resolution, std::vector<uint32_t>& out_indices);

    // Generate indices with LOD stitching
    static void generate_stitched_indices(uint32_t resolution, uint32_t lod,
                                          uint32_t north_lod, uint32_t south_lod,
                                          uint32_t east_lod, uint32_t west_lod,
                                          std::vector<uint32_t>& out_indices);

    // Generate indices for a specific LOD level
    static void generate_lod_indices(uint32_t resolution, uint32_t lod,
                                     std::vector<uint32_t>& out_indices);

    // Pregenerate all LOD index buffers
    static void pregenerate_all_lods(uint32_t base_resolution, uint32_t num_lods,
                                     std::vector<std::vector<uint32_t>>& out_index_buffers);

private:
    static void add_stitched_edge(std::vector<uint32_t>& indices, uint32_t resolution,
                                   uint32_t step, uint32_t neighbor_step,
                                   uint32_t edge, bool flip);
};

} // namespace engine::terrain
