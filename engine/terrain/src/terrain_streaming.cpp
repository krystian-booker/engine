#include <engine/terrain/terrain_streaming.hpp>
#include <engine/core/log.hpp>
#include <bgfx/bgfx.h>
#include <algorithm>
#include <cmath>
#include <chrono>

namespace engine::terrain {

using namespace engine::core;

// Terrain vertex layout (same as in terrain_renderer.cpp)
static bgfx::VertexLayout s_streaming_vertex_layout;
static bool s_streaming_layout_initialized = false;

static void init_streaming_vertex_layout() {
    if (s_streaming_layout_initialized) return;

    s_streaming_vertex_layout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Tangent, 4, bgfx::AttribType::Float)
        .end();

    s_streaming_layout_initialized = true;
}

TerrainStreamer::~TerrainStreamer() {
    shutdown();
}

void TerrainStreamer::init(const TerrainStreamingConfig& config,
                           const AABB& terrain_bounds,
                           const Heightmap* source_heightmap) {
    if (m_initialized) shutdown();

    m_config = config;
    m_terrain_bounds = terrain_bounds;
    m_source_heightmap = source_heightmap;

    // Calculate terrain scale from bounds
    m_terrain_scale = terrain_bounds.size();

    // Calculate grid dimensions
    float chunk_size = config.chunk_world_size;
    m_grid_min_x = static_cast<int32_t>(std::floor(terrain_bounds.min.x / chunk_size));
    m_grid_min_z = static_cast<int32_t>(std::floor(terrain_bounds.min.z / chunk_size));
    m_grid_max_x = static_cast<int32_t>(std::ceil(terrain_bounds.max.x / chunk_size));
    m_grid_max_z = static_cast<int32_t>(std::ceil(terrain_bounds.max.z / chunk_size));

    // Pre-create chunk entries (unloaded state)
    for (int32_t z = m_grid_min_z; z < m_grid_max_z; ++z) {
        for (int32_t x = m_grid_min_x; x < m_grid_max_x; ++x) {
            uint64_t key = make_chunk_key(x, z);

            StreamingChunk& chunk = m_chunks[key];
            chunk.grid_x = x;
            chunk.grid_z = z;

            // Calculate world bounds
            chunk.bounds.min = Vec3(
                x * chunk_size,
                terrain_bounds.min.y,
                z * chunk_size
            );
            chunk.bounds.max = Vec3(
                (x + 1) * chunk_size,
                terrain_bounds.max.y,
                (z + 1) * chunk_size
            );
            chunk.center = chunk.bounds.center();

            // Calculate heightmap region
            if (source_heightmap && source_heightmap->is_valid()) {
                float u_start = (chunk.bounds.min.x - terrain_bounds.min.x) / m_terrain_scale.x;
                float v_start = (chunk.bounds.min.z - terrain_bounds.min.z) / m_terrain_scale.z;

                chunk.heightmap_offset_x = static_cast<uint32_t>(
                    u_start * (source_heightmap->get_width() - 1));
                chunk.heightmap_offset_z = static_cast<uint32_t>(
                    v_start * (source_heightmap->get_height() - 1));
                chunk.heightmap_resolution = config.chunk_resolution;
            }
        }
    }

    init_streaming_vertex_layout();

    m_initialized = true;
    log(LogLevel::Info, "TerrainStreamer initialized: {}x{} chunks",
        m_grid_max_x - m_grid_min_x, m_grid_max_z - m_grid_min_z);
}

void TerrainStreamer::shutdown() {
    if (!m_initialized) return;

    // Wait for async loads to complete
    for (auto& task : m_async_loads) {
        if (task->future.valid()) {
            task->future.wait();
        }
    }
    m_async_loads.clear();

    // Unload all loaded chunks
    for (auto& [key, chunk] : m_chunks) {
        if (chunk.state == StreamingChunkState::Loaded) {
            unload_chunk(chunk);
        }
    }

    m_chunks.clear();
    m_loaded_count = 0;
    m_visible_count = 0;
    m_initialized = false;

    log(LogLevel::Info, "TerrainStreamer shutdown");
}

void TerrainStreamer::set_config(const TerrainStreamingConfig& config) {
    m_config = config;
}

void TerrainStreamer::update(const Vec3& camera_position, const Frustum& frustum) {
    if (!m_initialized) return;

    update_chunk_distances(camera_position);
    update_chunk_priorities();
    update_visibility(frustum);
    process_load_queue();
    process_unload_queue();
    check_async_loads();
}

void TerrainStreamer::update_chunk_distances(const Vec3& camera_pos) {
    for (auto& [key, chunk] : m_chunks) {
        Vec3 to_chunk = chunk.center - camera_pos;
        // Use XZ distance for streaming (ignore Y)
        chunk.distance_to_camera = std::sqrt(to_chunk.x * to_chunk.x +
                                              to_chunk.z * to_chunk.z);
    }
}

void TerrainStreamer::update_chunk_priorities() {
    // Clear queues
    while (!m_load_queue.empty()) m_load_queue.pop();
    m_unload_queue.clear();

    for (auto& [key, chunk] : m_chunks) {
        float dist = chunk.distance_to_camera;

        if (chunk.state == StreamingChunkState::Unloaded) {
            if (dist < m_config.load_distance) {
                // Queue for loading
                StreamingChunkRequest req;
                req.grid_x = chunk.grid_x;
                req.grid_z = chunk.grid_z;
                req.distance = dist;
                req.priority = m_config.load_distance - dist;  // Closer = higher priority
                m_load_queue.push(req);
            }
        } else if (chunk.state == StreamingChunkState::Loaded) {
            if (dist > m_config.unload_distance) {
                // Queue for unloading
                m_unload_queue.push_back(key);
            }
        }
    }

    // Sort unload queue by distance (furthest first)
    std::sort(m_unload_queue.begin(), m_unload_queue.end(),
        [this](uint64_t a, uint64_t b) {
            return m_chunks[a].distance_to_camera > m_chunks[b].distance_to_camera;
        });
}

void TerrainStreamer::update_visibility(const Frustum& frustum) {
    m_visible_count = 0;

    for (auto& [key, chunk] : m_chunks) {
        if (chunk.state == StreamingChunkState::Loaded) {
            chunk.visible = frustum.contains_aabb(chunk.bounds);
            if (chunk.visible) {
                m_visible_count++;
            }
        } else {
            chunk.visible = false;
        }
    }
}

void TerrainStreamer::process_load_queue() {
    uint32_t loads_this_frame = 0;

    while (!m_load_queue.empty() &&
           loads_this_frame < m_config.max_loads_per_frame &&
           m_loaded_count < m_config.max_loaded_chunks) {

        StreamingChunkRequest req = m_load_queue.top();
        m_load_queue.pop();

        uint64_t key = make_chunk_key(req.grid_x, req.grid_z);
        auto it = m_chunks.find(key);
        if (it == m_chunks.end()) continue;

        StreamingChunk& chunk = it->second;
        if (chunk.state != StreamingChunkState::Unloaded) continue;

        // Start async load
        chunk.state = StreamingChunkState::Loading;

        auto task = std::make_unique<AsyncChunkLoad>();
        task->chunk_key = key;

        // Capture what we need for async loading
        const Heightmap* hm = m_source_heightmap;
        Vec3 scale = m_terrain_scale;
        AABB bounds = m_terrain_bounds;
        uint32_t res = chunk.heightmap_resolution;
        AABB chunk_bounds = chunk.bounds;

        // Async load heightmap data
        task->future = std::async(std::launch::async, [hm, scale, bounds, res, chunk_bounds, &task]() -> bool {
            // Sample heightmap on background thread
            task->loaded_data.resize(res * res);

            if (!hm || !hm->is_valid()) return false;

            for (uint32_t z = 0; z < res; ++z) {
                for (uint32_t x = 0; x < res; ++x) {
                    float local_u = static_cast<float>(x) / (res - 1);
                    float local_v = static_cast<float>(z) / (res - 1);

                    float world_x = chunk_bounds.min.x + local_u * (chunk_bounds.max.x - chunk_bounds.min.x);
                    float world_z = chunk_bounds.min.z + local_v * (chunk_bounds.max.z - chunk_bounds.min.z);

                    float u = (world_x - bounds.min.x) / scale.x;
                    float v = (world_z - bounds.min.z) / scale.z;

                    task->loaded_data[z * res + x] = hm->sample(u, v);
                }
            }
            return true;
        });

        m_async_loads.push_back(std::move(task));
        loads_this_frame++;
    }
}

void TerrainStreamer::process_unload_queue() {
    uint32_t unloads_this_frame = 0;

    for (uint64_t key : m_unload_queue) {
        if (unloads_this_frame >= m_config.max_unloads_per_frame) break;

        auto it = m_chunks.find(key);
        if (it == m_chunks.end()) continue;

        StreamingChunk& chunk = it->second;
        if (chunk.state != StreamingChunkState::Loaded) continue;

        unload_chunk(chunk);
        unloads_this_frame++;
    }
}

void TerrainStreamer::check_async_loads() {
    for (auto it = m_async_loads.begin(); it != m_async_loads.end(); ) {
        auto& task = *it;

        if (task->future.wait_for(std::chrono::milliseconds(0)) ==
            std::future_status::ready) {

            bool success = task->future.get();

            auto chunk_it = m_chunks.find(task->chunk_key);
            if (chunk_it != m_chunks.end() && success) {
                StreamingChunk& chunk = chunk_it->second;
                chunk.height_data = std::move(task->loaded_data);
                upload_chunk_gpu(chunk);
                chunk.state = StreamingChunkState::Loaded;
                m_loaded_count++;
            } else if (chunk_it != m_chunks.end()) {
                // Load failed - mark as unloaded
                chunk_it->second.state = StreamingChunkState::Unloaded;
            }

            it = m_async_loads.erase(it);
        } else {
            ++it;
        }
    }
}

bool TerrainStreamer::load_chunk_data(StreamingChunk& chunk) {
    if (!m_source_heightmap || !m_source_heightmap->is_valid()) {
        return false;
    }

    sample_heightmap_region(chunk);
    return !chunk.height_data.empty();
}

void TerrainStreamer::sample_heightmap_region(StreamingChunk& chunk) {
    if (!m_source_heightmap || !m_source_heightmap->is_valid()) return;

    uint32_t res = chunk.heightmap_resolution;
    chunk.height_data.resize(res * res);

    for (uint32_t z = 0; z < res; ++z) {
        for (uint32_t x = 0; x < res; ++x) {
            float local_u = static_cast<float>(x) / (res - 1);
            float local_v = static_cast<float>(z) / (res - 1);

            float world_x = chunk.bounds.min.x + local_u * (chunk.bounds.max.x - chunk.bounds.min.x);
            float world_z = chunk.bounds.min.z + local_v * (chunk.bounds.max.z - chunk.bounds.min.z);

            float u = (world_x - m_terrain_bounds.min.x) / m_terrain_scale.x;
            float v = (world_z - m_terrain_bounds.min.z) / m_terrain_scale.z;

            chunk.height_data[z * res + x] = m_source_heightmap->sample(u, v);
        }
    }
}

void TerrainStreamer::upload_chunk_gpu(StreamingChunk& chunk) {
    if (chunk.height_data.empty()) return;

    uint32_t res = chunk.heightmap_resolution;
    float chunk_size_x = chunk.bounds.max.x - chunk.bounds.min.x;
    float chunk_size_z = chunk.bounds.max.z - chunk.bounds.min.z;

    // Build vertex data
    struct Vertex {
        Vec3 position;
        Vec3 normal;
        Vec2 uv;
        Vec4 tangent;
    };

    std::vector<Vertex> vertices;
    vertices.reserve(res * res);

    for (uint32_t z = 0; z < res; ++z) {
        for (uint32_t x = 0; x < res; ++x) {
            float local_u = static_cast<float>(x) / (res - 1);
            float local_v = static_cast<float>(z) / (res - 1);

            float world_x = chunk.bounds.min.x + local_u * chunk_size_x;
            float world_z = chunk.bounds.min.z + local_v * chunk_size_z;

            float height = chunk.height_data[z * res + x] * m_terrain_scale.y;

            // Calculate normal from neighboring heights
            float h_l = (x > 0) ? chunk.height_data[z * res + (x - 1)] : chunk.height_data[z * res + x];
            float h_r = (x < res - 1) ? chunk.height_data[z * res + (x + 1)] : chunk.height_data[z * res + x];
            float h_d = (z > 0) ? chunk.height_data[(z - 1) * res + x] : chunk.height_data[z * res + x];
            float h_u = (z < res - 1) ? chunk.height_data[(z + 1) * res + x] : chunk.height_data[z * res + x];

            float dx = (h_r - h_l) * m_terrain_scale.y;
            float dz = (h_u - h_d) * m_terrain_scale.y;
            Vec3 normal = normalize(Vec3(-dx, 2.0f * chunk_size_x / (res - 1), -dz));

            // Calculate tangent
            Vec3 up(0.0f, 1.0f, 0.0f);
            Vec3 tangent = cross(up, normal);
            if (length(tangent) < 0.001f) {
                tangent = Vec3(1.0f, 0.0f, 0.0f);
            }
            tangent = normalize(tangent);

            Vertex v;
            v.position = Vec3(world_x, height, world_z);
            v.normal = normal;
            v.uv = Vec2(
                (world_x - m_terrain_bounds.min.x) / m_terrain_scale.x,
                (world_z - m_terrain_bounds.min.z) / m_terrain_scale.z
            );
            v.tangent = Vec4(tangent.x, tangent.y, tangent.z, 1.0f);

            vertices.push_back(v);

            // Update chunk height bounds
            chunk.bounds.min.y = std::min(chunk.bounds.min.y, height);
            chunk.bounds.max.y = std::max(chunk.bounds.max.y, height);
        }
    }

    // Build index data
    std::vector<uint32_t> indices;
    indices.reserve((res - 1) * (res - 1) * 6);

    for (uint32_t z = 0; z < res - 1; ++z) {
        for (uint32_t x = 0; x < res - 1; ++x) {
            uint32_t i00 = z * res + x;
            uint32_t i10 = i00 + 1;
            uint32_t i01 = i00 + res;
            uint32_t i11 = i01 + 1;

            indices.push_back(i00);
            indices.push_back(i01);
            indices.push_back(i10);

            indices.push_back(i10);
            indices.push_back(i01);
            indices.push_back(i11);
        }
    }

    // Create vertex buffer
    const bgfx::Memory* vb_mem = bgfx::copy(
        vertices.data(),
        static_cast<uint32_t>(vertices.size() * sizeof(Vertex))
    );
    bgfx::VertexBufferHandle vb = bgfx::createVertexBuffer(vb_mem, s_streaming_vertex_layout);
    chunk.vertex_buffer = vb.idx;

    // Create index buffer
    const bgfx::Memory* ib_mem = bgfx::copy(
        indices.data(),
        static_cast<uint32_t>(indices.size() * sizeof(uint32_t))
    );
    bgfx::IndexBufferHandle ib = bgfx::createIndexBuffer(ib_mem, BGFX_BUFFER_INDEX32);
    chunk.index_buffer = ib.idx;
}

void TerrainStreamer::unload_chunk(StreamingChunk& chunk) {
    // Destroy GPU resources
    if (chunk.vertex_buffer != UINT32_MAX) {
        bgfx::VertexBufferHandle vb = { static_cast<uint16_t>(chunk.vertex_buffer) };
        if (bgfx::isValid(vb)) {
            bgfx::destroy(vb);
        }
        chunk.vertex_buffer = UINT32_MAX;
    }

    if (chunk.index_buffer != UINT32_MAX) {
        bgfx::IndexBufferHandle ib = { static_cast<uint16_t>(chunk.index_buffer) };
        if (bgfx::isValid(ib)) {
            bgfx::destroy(ib);
        }
        chunk.index_buffer = UINT32_MAX;
    }

    // Clear CPU data
    chunk.height_data.clear();
    chunk.height_data.shrink_to_fit();

    chunk.state = StreamingChunkState::Unloaded;
    chunk.visible = false;
    m_loaded_count--;
}

void TerrainStreamer::get_visible_chunks(std::vector<const StreamingChunk*>& out_chunks) const {
    out_chunks.clear();
    for (const auto& [key, chunk] : m_chunks) {
        if (chunk.visible && chunk.state == StreamingChunkState::Loaded) {
            out_chunks.push_back(&chunk);
        }
    }
}

void TerrainStreamer::get_loaded_chunks(std::vector<const StreamingChunk*>& out_chunks) const {
    out_chunks.clear();
    for (const auto& [key, chunk] : m_chunks) {
        if (chunk.state == StreamingChunkState::Loaded) {
            out_chunks.push_back(&chunk);
        }
    }
}

void TerrainStreamer::request_load(int32_t grid_x, int32_t grid_z) {
    uint64_t key = make_chunk_key(grid_x, grid_z);
    auto it = m_chunks.find(key);
    if (it == m_chunks.end()) return;

    StreamingChunk& chunk = it->second;
    if (chunk.state == StreamingChunkState::Unloaded) {
        StreamingChunkRequest req;
        req.grid_x = grid_x;
        req.grid_z = grid_z;
        req.distance = chunk.distance_to_camera;
        req.priority = m_config.load_distance;  // High priority
        m_load_queue.push(req);
    }
}

void TerrainStreamer::request_unload(int32_t grid_x, int32_t grid_z) {
    uint64_t key = make_chunk_key(grid_x, grid_z);
    m_unload_queue.push_back(key);
}

void TerrainStreamer::force_load_sync(int32_t grid_x, int32_t grid_z) {
    uint64_t key = make_chunk_key(grid_x, grid_z);
    auto it = m_chunks.find(key);
    if (it == m_chunks.end()) return;

    StreamingChunk& chunk = it->second;
    if (chunk.state != StreamingChunkState::Unloaded) return;

    chunk.state = StreamingChunkState::Loading;

    if (load_chunk_data(chunk)) {
        upload_chunk_gpu(chunk);
        chunk.state = StreamingChunkState::Loaded;
        m_loaded_count++;
    } else {
        chunk.state = StreamingChunkState::Unloaded;
    }
}

float TerrainStreamer::get_memory_usage_mb() const {
    float total_bytes = 0.0f;

    for (const auto& [key, chunk] : m_chunks) {
        if (chunk.state == StreamingChunkState::Loaded) {
            // Height data (CPU)
            total_bytes += chunk.height_data.size() * sizeof(float);

            // Vertex buffer (GPU) - estimated
            uint32_t vertex_count = chunk.heightmap_resolution * chunk.heightmap_resolution;
            total_bytes += vertex_count * (3 + 3 + 2 + 4) * sizeof(float);  // pos + normal + uv + tangent

            // Index buffer (GPU) - estimated
            uint32_t index_count = (chunk.heightmap_resolution - 1) * (chunk.heightmap_resolution - 1) * 6;
            total_bytes += index_count * sizeof(uint32_t);
        }
    }

    return total_bytes / (1024.0f * 1024.0f);
}

} // namespace engine::terrain
