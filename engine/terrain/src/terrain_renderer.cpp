#include <engine/terrain/terrain_renderer.hpp>
#include <algorithm>
#include <cmath>

namespace engine::terrain {

TerrainRenderer::~TerrainRenderer() {
    shutdown();
}

void TerrainRenderer::init(const Heightmap& heightmap, const Vec3& terrain_scale,
                            const TerrainRenderSettings& settings) {
    if (m_initialized) shutdown();

    m_heightmap = &heightmap;
    m_terrain_scale = terrain_scale;
    m_settings = settings;

    // Calculate terrain bounds
    m_terrain_bounds.min = Vec3(0.0f);
    m_terrain_bounds.max = terrain_scale;

    // Initialize LOD selector
    m_lod_selector.set_settings(settings.lod_settings);

    // Generate mesh data
    generate_mesh();

    // Pregenerate LOD index buffers
    TerrainIndexGenerator::pregenerate_all_lods(
        settings.chunk_resolution,
        settings.lod_settings.num_lods,
        m_lod_index_buffers
    );

    // Create GPU resources
    create_gpu_resources();

    m_initialized = true;
}

void TerrainRenderer::shutdown() {
    if (!m_initialized) return;

    destroy_gpu_resources();

    m_vertices.clear();
    m_indices.clear();
    m_chunks.clear();
    m_lod_index_buffers.clear();

    m_heightmap = nullptr;
    m_splat_map = nullptr;
    m_initialized = false;
}

void TerrainRenderer::set_settings(const TerrainRenderSettings& settings) {
    m_settings = settings;
    m_lod_selector.set_settings(settings.lod_settings);
}

void TerrainRenderer::set_layer(uint32_t index, const TerrainLayer& layer) {
    if (index < MAX_TERRAIN_LAYERS) {
        m_layers[index] = layer;
        m_active_layer_count = std::max(m_active_layer_count, index + 1);
    }
}

const TerrainLayer& TerrainRenderer::get_layer(uint32_t index) const {
    static TerrainLayer empty;
    return index < MAX_TERRAIN_LAYERS ? m_layers[index] : empty;
}

void TerrainRenderer::set_splat_map(const SplatMap& splat_map) {
    m_splat_map = &splat_map;
}

void TerrainRenderer::update(const Vec3& camera_position, const Frustum& frustum) {
    if (!m_initialized) return;

    update_chunk_lods(camera_position);
    update_visibility(frustum);
}

void TerrainRenderer::render(uint16_t view_id) {
    if (!m_initialized) return;

    // Render would use bgfx here
    // For each visible chunk, submit draw call with appropriate LOD
}

void TerrainRenderer::render_shadow(uint16_t view_id) {
    if (!m_initialized || !m_settings.cast_shadows) return;

    // Render shadow pass
}

float TerrainRenderer::get_height_at(float world_x, float world_z) const {
    if (!m_heightmap || !m_heightmap->is_valid()) return 0.0f;

    return m_heightmap->sample_world(world_x, world_z, m_terrain_scale);
}

Vec3 TerrainRenderer::get_normal_at(float world_x, float world_z) const {
    if (!m_heightmap || !m_heightmap->is_valid()) return Vec3(0.0f, 1.0f, 0.0f);

    return m_heightmap->calculate_normal_world(world_x, world_z, m_terrain_scale);
}

bool TerrainRenderer::raycast(const Vec3& origin, const Vec3& direction, float max_dist,
                               Vec3& out_hit, Vec3& out_normal) const {
    if (!m_heightmap || !m_heightmap->is_valid()) return false;

    // Simple stepping raycast
    const float step_size = 0.5f;
    const int max_steps = static_cast<int>(max_dist / step_size);

    Vec3 pos = origin;
    Vec3 prev_pos = origin;

    for (int i = 0; i < max_steps; ++i) {
        prev_pos = pos;
        pos = pos + direction * step_size;

        // Check if we're within terrain bounds
        if (pos.x < 0.0f || pos.x > m_terrain_scale.x ||
            pos.z < 0.0f || pos.z > m_terrain_scale.z) {
            continue;
        }

        float terrain_height = get_height_at(pos.x, pos.z);

        if (pos.y <= terrain_height) {
            // Binary search for precise intersection
            Vec3 lo = prev_pos;
            Vec3 hi = pos;

            for (int j = 0; j < 10; ++j) {
                Vec3 mid = (lo + hi) * 0.5f;
                float mid_height = get_height_at(mid.x, mid.z);

                if (mid.y > mid_height) {
                    lo = mid;
                } else {
                    hi = mid;
                }
            }

            out_hit = (lo + hi) * 0.5f;
            out_hit.y = get_height_at(out_hit.x, out_hit.z);
            out_normal = get_normal_at(out_hit.x, out_hit.z);
            return true;
        }
    }

    return false;
}

void TerrainRenderer::generate_mesh() {
    if (!m_heightmap || !m_heightmap->is_valid()) return;

    uint32_t chunks_per_side = m_settings.chunks_per_side;
    uint32_t chunk_resolution = m_settings.chunk_resolution;

    // Calculate chunk size in world units
    float chunk_size_x = m_terrain_scale.x / chunks_per_side;
    float chunk_size_z = m_terrain_scale.z / chunks_per_side;

    // Generate chunks
    m_chunks.resize(chunks_per_side * chunks_per_side);

    for (uint32_t cz = 0; cz < chunks_per_side; ++cz) {
        for (uint32_t cx = 0; cx < chunks_per_side; ++cx) {
            uint32_t chunk_idx = cz * chunks_per_side + cx;
            generate_chunk(cx, cz);
        }
    }
}

void TerrainRenderer::generate_chunk(uint32_t chunk_x, uint32_t chunk_z) {
    uint32_t chunks_per_side = m_settings.chunks_per_side;
    uint32_t chunk_resolution = m_settings.chunk_resolution;

    float chunk_size_x = m_terrain_scale.x / chunks_per_side;
    float chunk_size_z = m_terrain_scale.z / chunks_per_side;

    uint32_t chunk_idx = chunk_z * chunks_per_side + chunk_x;
    TerrainChunk& chunk = m_chunks[chunk_idx];

    chunk.grid_x = chunk_x;
    chunk.grid_z = chunk_z;

    // Calculate world bounds
    float min_x = chunk_x * chunk_size_x;
    float min_z = chunk_z * chunk_size_z;
    float max_x = min_x + chunk_size_x;
    float max_z = min_z + chunk_size_z;

    chunk.bounds.min = Vec3(min_x, 0.0f, min_z);
    chunk.bounds.max = Vec3(max_x, m_terrain_scale.y, max_z);
    chunk.center = (chunk.bounds.min + chunk.bounds.max) * 0.5f;

    // Generate vertices for this chunk
    uint32_t vertex_offset = static_cast<uint32_t>(m_vertices.size());

    for (uint32_t z = 0; z < chunk_resolution; ++z) {
        for (uint32_t x = 0; x < chunk_resolution; ++x) {
            float local_u = static_cast<float>(x) / (chunk_resolution - 1);
            float local_v = static_cast<float>(z) / (chunk_resolution - 1);

            float world_x = min_x + local_u * chunk_size_x;
            float world_z = min_z + local_v * chunk_size_z;

            float global_u = world_x / m_terrain_scale.x;
            float global_v = world_z / m_terrain_scale.z;

            float height = m_heightmap->sample(global_u, global_v) * m_terrain_scale.y;

            TerrainVertex vertex;
            vertex.position = Vec3(world_x, height, world_z);
            vertex.normal = m_heightmap->calculate_normal(global_u, global_v,
                                                           m_terrain_scale.x, m_terrain_scale.y);
            vertex.uv = Vec2(global_u, global_v);

            // Calculate tangent
            Vec3 up(0.0f, 1.0f, 0.0f);
            Vec3 tangent = cross(up, vertex.normal);
            if (length(tangent) < 0.001f) {
                tangent = Vec3(1.0f, 0.0f, 0.0f);
            }
            tangent = normalize(tangent);
            vertex.tangent = Vec4(tangent.x, tangent.y, tangent.z, 1.0f);

            m_vertices.push_back(vertex);

            // Update chunk height bounds
            chunk.bounds.min.y = std::min(chunk.bounds.min.y, height);
            chunk.bounds.max.y = std::max(chunk.bounds.max.y, height);
        }
    }

    // Generate indices
    chunk.index_offset = static_cast<uint32_t>(m_indices.size());

    for (uint32_t z = 0; z < chunk_resolution - 1; ++z) {
        for (uint32_t x = 0; x < chunk_resolution - 1; ++x) {
            uint32_t i00 = vertex_offset + z * chunk_resolution + x;
            uint32_t i10 = i00 + 1;
            uint32_t i01 = i00 + chunk_resolution;
            uint32_t i11 = i01 + 1;

            // Triangle 1
            m_indices.push_back(i00);
            m_indices.push_back(i01);
            m_indices.push_back(i10);

            // Triangle 2
            m_indices.push_back(i10);
            m_indices.push_back(i01);
            m_indices.push_back(i11);
        }
    }

    chunk.index_count = static_cast<uint32_t>(m_indices.size()) - chunk.index_offset;
}

void TerrainRenderer::update_chunk_lods(const Vec3& camera_pos) {
    for (auto& chunk : m_chunks) {
        chunk.lod = m_lod_selector.calculate_lod(chunk.center, camera_pos);
    }

    // Update neighbor LOD info for stitching
    uint32_t chunks_per_side = m_settings.chunks_per_side;

    for (uint32_t z = 0; z < chunks_per_side; ++z) {
        for (uint32_t x = 0; x < chunks_per_side; ++x) {
            uint32_t idx = z * chunks_per_side + x;
            TerrainChunk& chunk = m_chunks[idx];

            // North neighbor
            if (z > 0) {
                chunk.lod.north_lod = m_chunks[(z - 1) * chunks_per_side + x].lod.lod_level;
            }
            // South neighbor
            if (z < chunks_per_side - 1) {
                chunk.lod.south_lod = m_chunks[(z + 1) * chunks_per_side + x].lod.lod_level;
            }
            // East neighbor
            if (x < chunks_per_side - 1) {
                chunk.lod.east_lod = m_chunks[z * chunks_per_side + x + 1].lod.lod_level;
            }
            // West neighbor
            if (x > 0) {
                chunk.lod.west_lod = m_chunks[z * chunks_per_side + x - 1].lod.lod_level;
            }

            // Check if stitching is needed
            chunk.lod.needs_stitch =
                chunk.lod.north_lod != chunk.lod.lod_level ||
                chunk.lod.south_lod != chunk.lod.lod_level ||
                chunk.lod.east_lod != chunk.lod.lod_level ||
                chunk.lod.west_lod != chunk.lod.lod_level;
        }
    }
}

void TerrainRenderer::update_visibility(const Frustum& frustum) {
    m_visible_chunk_count = 0;

    for (auto& chunk : m_chunks) {
        chunk.in_frustum = frustum.intersects(chunk.bounds);
        chunk.visible = chunk.in_frustum;

        if (chunk.visible) {
            m_visible_chunk_count++;
        }
    }
}

void TerrainRenderer::rebuild_index_buffer() {
    // Rebuild index buffer with LOD-appropriate indices and stitching
}

void TerrainRenderer::create_gpu_resources() {
    // Create bgfx resources
}

void TerrainRenderer::destroy_gpu_resources() {
    // Destroy bgfx resources
}

void TerrainRenderer::upload_chunk_data(const TerrainChunk& chunk) {
    // Upload chunk data to GPU
}

// TerrainLODSelector implementation

ChunkLOD TerrainLODSelector::calculate_lod(const Vec3& chunk_center, const Vec3& camera_pos) const {
    ChunkLOD result;
    result.distance_to_camera = length(chunk_center - camera_pos);
    result.lod_level = get_lod_for_distance(result.distance_to_camera);

    if (m_settings.use_geomorphing) {
        result.morph_factor = calculate_morph_factor(result.distance_to_camera, result.lod_level);
    }

    return result;
}

uint32_t TerrainLODSelector::get_lod_for_distance(float distance) const {
    for (uint32_t lod = 0; lod < m_settings.num_lods - 1; ++lod) {
        if (distance < get_lod_end_distance(lod)) {
            return lod;
        }
    }
    return m_settings.num_lods - 1;
}

float TerrainLODSelector::get_lod_start_distance(uint32_t lod) const {
    if (lod == 0) return 0.0f;
    return m_settings.base_lod_distance * std::pow(m_settings.lod_distance_ratio, static_cast<float>(lod - 1));
}

float TerrainLODSelector::get_lod_end_distance(uint32_t lod) const {
    return m_settings.base_lod_distance * std::pow(m_settings.lod_distance_ratio, static_cast<float>(lod));
}

float TerrainLODSelector::calculate_morph_factor(float distance, uint32_t lod) const {
    float start = get_lod_start_distance(lod);
    float end = get_lod_end_distance(lod);
    float morph_start = end - (end - start) * m_settings.morph_range;

    if (distance < morph_start) return 0.0f;
    if (distance >= end) return 1.0f;

    return (distance - morph_start) / (end - morph_start);
}

// TerrainIndexGenerator implementation

void TerrainIndexGenerator::generate_grid_indices(uint32_t resolution, std::vector<uint32_t>& out_indices) {
    out_indices.clear();
    out_indices.reserve((resolution - 1) * (resolution - 1) * 6);

    for (uint32_t z = 0; z < resolution - 1; ++z) {
        for (uint32_t x = 0; x < resolution - 1; ++x) {
            uint32_t i00 = z * resolution + x;
            uint32_t i10 = i00 + 1;
            uint32_t i01 = i00 + resolution;
            uint32_t i11 = i01 + 1;

            out_indices.push_back(i00);
            out_indices.push_back(i01);
            out_indices.push_back(i10);

            out_indices.push_back(i10);
            out_indices.push_back(i01);
            out_indices.push_back(i11);
        }
    }
}

void TerrainIndexGenerator::generate_lod_indices(uint32_t resolution, uint32_t lod,
                                                  std::vector<uint32_t>& out_indices) {
    out_indices.clear();

    uint32_t step = 1u << lod;  // 2^lod
    uint32_t lod_resolution = (resolution - 1) / step + 1;

    out_indices.reserve((lod_resolution - 1) * (lod_resolution - 1) * 6);

    for (uint32_t z = 0; z < resolution - 1; z += step) {
        for (uint32_t x = 0; x < resolution - 1; x += step) {
            uint32_t i00 = z * resolution + x;
            uint32_t i10 = i00 + step;
            uint32_t i01 = i00 + step * resolution;
            uint32_t i11 = i01 + step;

            // Clamp to grid bounds
            i10 = std::min(i10, z * resolution + resolution - 1);
            i01 = std::min(i01, (resolution - 1) * resolution + x);
            i11 = std::min(i11, (resolution - 1) * resolution + resolution - 1);

            out_indices.push_back(i00);
            out_indices.push_back(i01);
            out_indices.push_back(i10);

            out_indices.push_back(i10);
            out_indices.push_back(i01);
            out_indices.push_back(i11);
        }
    }
}

void TerrainIndexGenerator::generate_stitched_indices(uint32_t resolution, uint32_t lod,
                                                       uint32_t north_lod, uint32_t south_lod,
                                                       uint32_t east_lod, uint32_t west_lod,
                                                       std::vector<uint32_t>& out_indices) {
    // Start with base LOD indices
    generate_lod_indices(resolution, lod, out_indices);

    // Add stitching for edges with different LOD neighbors
    // This is a simplified implementation - full implementation would handle
    // T-junction fixes at edges
}

void TerrainIndexGenerator::pregenerate_all_lods(uint32_t base_resolution, uint32_t num_lods,
                                                  std::vector<std::vector<uint32_t>>& out_index_buffers) {
    out_index_buffers.resize(num_lods);

    for (uint32_t lod = 0; lod < num_lods; ++lod) {
        generate_lod_indices(base_resolution, lod, out_index_buffers[lod]);
    }
}

// TerrainPhysicsGenerator implementation

void TerrainPhysicsGenerator::generate_collision_mesh(const Heightmap& heightmap, const Vec3& terrain_scale,
                                                       std::vector<Vec3>& out_vertices,
                                                       std::vector<uint32_t>& out_indices,
                                                       uint32_t resolution) {
    if (!heightmap.is_valid()) return;

    if (resolution == 0) {
        resolution = heightmap.get_width();
    }

    out_vertices.clear();
    out_indices.clear();
    out_vertices.reserve(resolution * resolution);
    out_indices.reserve((resolution - 1) * (resolution - 1) * 6);

    // Generate vertices
    for (uint32_t z = 0; z < resolution; ++z) {
        for (uint32_t x = 0; x < resolution; ++x) {
            float u = static_cast<float>(x) / (resolution - 1);
            float v = static_cast<float>(z) / (resolution - 1);

            float height = heightmap.sample(u, v) * terrain_scale.y;

            out_vertices.push_back(Vec3(
                u * terrain_scale.x,
                height,
                v * terrain_scale.z
            ));
        }
    }

    // Generate indices
    for (uint32_t z = 0; z < resolution - 1; ++z) {
        for (uint32_t x = 0; x < resolution - 1; ++x) {
            uint32_t i00 = z * resolution + x;
            uint32_t i10 = i00 + 1;
            uint32_t i01 = i00 + resolution;
            uint32_t i11 = i01 + 1;

            out_indices.push_back(i00);
            out_indices.push_back(i01);
            out_indices.push_back(i10);

            out_indices.push_back(i10);
            out_indices.push_back(i01);
            out_indices.push_back(i11);
        }
    }
}

void TerrainPhysicsGenerator::generate_simplified_mesh(const Heightmap& heightmap, const Vec3& terrain_scale,
                                                        float simplification_ratio,
                                                        std::vector<Vec3>& out_vertices,
                                                        std::vector<uint32_t>& out_indices) {
    uint32_t resolution = static_cast<uint32_t>(heightmap.get_width() * simplification_ratio);
    resolution = std::max(resolution, 2u);
    generate_collision_mesh(heightmap, terrain_scale, out_vertices, out_indices, resolution);
}

void TerrainPhysicsGenerator::generate_height_field(const Heightmap& heightmap, const Vec3& terrain_scale,
                                                     std::vector<float>& out_heights,
                                                     uint32_t& out_rows, uint32_t& out_cols) {
    if (!heightmap.is_valid()) {
        out_rows = 0;
        out_cols = 0;
        return;
    }

    out_cols = heightmap.get_width();
    out_rows = heightmap.get_height();
    out_heights.resize(out_cols * out_rows);

    for (uint32_t i = 0; i < out_heights.size(); ++i) {
        out_heights[i] = heightmap.get_data()[i] * terrain_scale.y;
    }
}

} // namespace engine::terrain
