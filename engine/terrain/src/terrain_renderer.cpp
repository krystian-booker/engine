#include <engine/terrain/terrain_renderer.hpp>
#include <engine/terrain/terrain_streaming.hpp>
#include <engine/core/log.hpp>
#include <bgfx/bgfx.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <unordered_map>

namespace engine::terrain {

using namespace engine::core;

// Helper function to get platform-specific shader path
static std::string get_shader_path() {
#if BX_PLATFORM_WINDOWS
    bgfx::RendererType::Enum renderer = bgfx::getRendererType();
    if (renderer == bgfx::RendererType::Direct3D11 || renderer == bgfx::RendererType::Direct3D12) {
        return "shaders/dx11/";
    } else if (renderer == bgfx::RendererType::Vulkan) {
        return "shaders/spirv/";
    }
    return "shaders/dx11/";
#elif BX_PLATFORM_LINUX
    bgfx::RendererType::Enum renderer = bgfx::getRendererType();
    if (renderer == bgfx::RendererType::Vulkan) {
        return "shaders/spirv/";
    }
    return "shaders/glsl/";
#elif BX_PLATFORM_OSX
    return "shaders/metal/";
#else
    return "shaders/glsl/";
#endif
}

// Load shader from file
static bgfx::ShaderHandle load_terrain_shader(const std::string& name) {
    std::string path = get_shader_path() + name + ".bin";
    std::ifstream file(path, std::ios::binary | std::ios::ate);

    if (!file.is_open()) {
        log(LogLevel::Warn, "Terrain shader not found: {}", path);
        return BGFX_INVALID_HANDLE;
    }

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    const bgfx::Memory* mem = bgfx::alloc(static_cast<uint32_t>(size) + 1);
    file.read(reinterpret_cast<char*>(mem->data), size);
    mem->data[size] = '\0';

    return bgfx::createShader(mem);
}

// Terrain vertex layout
static bgfx::VertexLayout s_terrain_vertex_layout;
static bool s_layout_initialized = false;

static void init_vertex_layout() {
    if (s_layout_initialized) return;

    s_terrain_vertex_layout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Tangent, 4, bgfx::AttribType::Float)
        .end();

    s_layout_initialized = true;
}

// Internal storage for bgfx handles
struct TerrainGPUResources {
    bgfx::VertexBufferHandle vertex_buffer = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle index_buffer = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle splat_texture = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle shadow_program = BGFX_INVALID_HANDLE;

    // Per-chunk dynamic index buffers (for LOD)
    std::vector<bgfx::DynamicIndexBufferHandle> chunk_index_buffers;

    // Uniform handles
    bgfx::UniformHandle u_terrainParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_layer0Params = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_layer1Params = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_layer2Params = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_layer3Params = BGFX_INVALID_HANDLE;

    // Sampler handles
    bgfx::UniformHandle s_splatMap = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_layer0Albedo = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_layer0Normal = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_layer0ARM = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_layer1Albedo = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_layer1Normal = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_layer1ARM = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_layer2Albedo = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_layer2Normal = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_layer2ARM = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_layer3Albedo = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_layer3Normal = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_layer3ARM = BGFX_INVALID_HANDLE;
};

// Use unique_ptr to manage GPU resources
static std::unordered_map<TerrainRenderer*, std::unique_ptr<TerrainGPUResources>> s_gpu_resources;

TerrainRenderer::TerrainRenderer() = default;

TerrainRenderer::~TerrainRenderer() {
    shutdown();
}

void TerrainRenderer::init(const Heightmap& heightmap, const Vec3& terrain_scale,
                            const TerrainRenderSettings& settings) {
    if (m_initialized) shutdown();

    m_heightmap = &heightmap;
    m_terrain_scale = terrain_scale;
    m_settings = settings;
    m_use_quadtree = settings.use_quadtree_lod;

    // Calculate terrain bounds
    m_terrain_bounds.min = Vec3(0.0f);
    m_terrain_bounds.max = terrain_scale;

    // Initialize LOD selector
    m_lod_selector.set_settings(settings.lod_settings);

    // Initialize quadtree if enabled
    if (m_use_quadtree) {
        m_quadtree.build(m_terrain_bounds, settings.quadtree_max_depth);
    }

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

    // Use streaming mode if enabled
    if (m_streamer) {
        m_streamer->update(camera_position, frustum);
        return;
    }

    // Use quadtree LOD if enabled
    if (m_use_quadtree) {
        m_quadtree.update(camera_position, m_settings.lod_settings.base_lod_distance);
        // Quadtree handles its own visibility via get_visible_chunks()
        return;
    }

    // Standard uniform grid LOD update
    update_chunk_lods(camera_position);
    update_visibility(frustum);
    rebuild_index_buffer();
}

void TerrainRenderer::render(uint16_t view_id) {
    if (!m_initialized) return;

    auto it = s_gpu_resources.find(this);
    if (it == s_gpu_resources.end()) return;

    TerrainGPUResources* gpu = it->second.get();
    if (!bgfx::isValid(gpu->vertex_buffer) || !bgfx::isValid(gpu->index_buffer)) return;

    // Set terrain uniforms
    float terrain_params[4] = { 0.1f, 0.0f, 0.0f, 0.0f };  // tile scale
    bgfx::setUniform(gpu->u_terrainParams, terrain_params);

    // Set layer params (uv_scale, metallic, roughness, ao multipliers)
    for (uint32_t i = 0; i < 4; ++i) {
        const TerrainLayer& layer = m_layers[i];
        float layer_params[4] = { layer.uv_scale, layer.metallic, layer.roughness, 1.0f };

        bgfx::UniformHandle u_layerParams = BGFX_INVALID_HANDLE;
        switch (i) {
            case 0: u_layerParams = gpu->u_layer0Params; break;
            case 1: u_layerParams = gpu->u_layer1Params; break;
            case 2: u_layerParams = gpu->u_layer2Params; break;
            case 3: u_layerParams = gpu->u_layer3Params; break;
        }
        if (bgfx::isValid(u_layerParams)) {
            bgfx::setUniform(u_layerParams, layer_params);
        }
    }

    // Set splat map texture
    if (bgfx::isValid(gpu->splat_texture) && bgfx::isValid(gpu->s_splatMap)) {
        bgfx::setTexture(0, gpu->s_splatMap, gpu->splat_texture);
    }

    // Set layer textures (if available)
    // Layer 0
    if (m_layers[0].albedo_texture != UINT32_MAX && bgfx::isValid(gpu->s_layer0Albedo)) {
        bgfx::TextureHandle tex = { static_cast<uint16_t>(m_layers[0].albedo_texture) };
        bgfx::setTexture(1, gpu->s_layer0Albedo, tex);
    }
    if (m_layers[0].normal_texture != UINT32_MAX && bgfx::isValid(gpu->s_layer0Normal)) {
        bgfx::TextureHandle tex = { static_cast<uint16_t>(m_layers[0].normal_texture) };
        bgfx::setTexture(2, gpu->s_layer0Normal, tex);
    }
    if (m_layers[0].arm_texture != UINT32_MAX && bgfx::isValid(gpu->s_layer0ARM)) {
        bgfx::TextureHandle tex = { static_cast<uint16_t>(m_layers[0].arm_texture) };
        bgfx::setTexture(3, gpu->s_layer0ARM, tex);
    }

    // For each visible chunk, submit draw call
    for (size_t i = 0; i < m_chunks.size(); ++i) {
        const auto& chunk = m_chunks[i];
        if (!chunk.visible) continue;

        // Set vertex buffer
        bgfx::setVertexBuffer(0, gpu->vertex_buffer);

        // Use per-chunk dynamic index buffer if available, otherwise fall back to static
        if (i < gpu->chunk_index_buffers.size() && bgfx::isValid(gpu->chunk_index_buffers[i])) {
            bgfx::setIndexBuffer(gpu->chunk_index_buffers[i]);
        } else {
            bgfx::setIndexBuffer(gpu->index_buffer, chunk.index_offset, chunk.index_count);
        }

        // Set render state
        uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
                       | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS
                       | BGFX_STATE_CULL_CCW | BGFX_STATE_MSAA;

        if (m_wireframe) {
            state |= BGFX_STATE_PT_LINES;
        }

        bgfx::setState(state);

        // Submit draw call
        if (bgfx::isValid(gpu->program)) {
            bgfx::submit(view_id, gpu->program);
        }
    }
}

void TerrainRenderer::render_shadow(uint16_t view_id) {
    if (!m_initialized || !m_settings.cast_shadows) return;

    auto it = s_gpu_resources.find(this);
    if (it == s_gpu_resources.end()) return;

    TerrainGPUResources* gpu = it->second.get();
    if (!bgfx::isValid(gpu->vertex_buffer) || !bgfx::isValid(gpu->index_buffer)) return;

    // For shadow pass, render all visible chunks with shadow program
    for (size_t i = 0; i < m_chunks.size(); ++i) {
        const auto& chunk = m_chunks[i];
        if (!chunk.visible) continue;

        bgfx::setVertexBuffer(0, gpu->vertex_buffer);

        // Use per-chunk dynamic index buffer if available
        if (i < gpu->chunk_index_buffers.size() && bgfx::isValid(gpu->chunk_index_buffers[i])) {
            bgfx::setIndexBuffer(gpu->chunk_index_buffers[i]);
        } else {
            bgfx::setIndexBuffer(gpu->index_buffer, chunk.index_offset, chunk.index_count);
        }

        uint64_t state = BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_CULL_CCW;
        bgfx::setState(state);

        if (bgfx::isValid(gpu->shadow_program)) {
            bgfx::submit(view_id, gpu->shadow_program);
        }
    }
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
        chunk.in_frustum = frustum.contains_aabb(chunk.bounds);
        chunk.visible = chunk.in_frustum;

        if (chunk.visible) {
            m_visible_chunk_count++;
        }
    }
}

void TerrainRenderer::rebuild_index_buffer() {
    auto it = s_gpu_resources.find(this);
    if (it == s_gpu_resources.end()) return;

    TerrainGPUResources* gpu = it->second.get();
    if (gpu->chunk_index_buffers.size() != m_chunks.size()) return;

    std::vector<uint32_t> indices;
    uint32_t resolution = m_settings.chunk_resolution;

    for (size_t i = 0; i < m_chunks.size(); ++i) {
        const TerrainChunk& chunk = m_chunks[i];

        if (!bgfx::isValid(gpu->chunk_index_buffers[i])) continue;

        // Generate indices with appropriate LOD level and stitching
        if (chunk.lod.needs_stitch) {
            TerrainIndexGenerator::generate_stitched_indices(
                resolution,
                chunk.lod.lod_level,
                chunk.lod.north_lod,
                chunk.lod.south_lod,
                chunk.lod.east_lod,
                chunk.lod.west_lod,
                indices
            );
        } else {
            TerrainIndexGenerator::generate_lod_indices(resolution, chunk.lod.lod_level, indices);
        }

        if (!indices.empty()) {
            const bgfx::Memory* mem = bgfx::copy(
                indices.data(),
                static_cast<uint32_t>(indices.size() * sizeof(uint32_t))
            );
            bgfx::update(gpu->chunk_index_buffers[i], 0, mem);
        }
    }
}

void TerrainRenderer::create_gpu_resources() {
    init_vertex_layout();

    // Create GPU resources struct
    auto gpu = std::make_unique<TerrainGPUResources>();

    // Create vertex buffer
    if (!m_vertices.empty()) {
        const bgfx::Memory* vb_mem = bgfx::copy(
            m_vertices.data(),
            static_cast<uint32_t>(m_vertices.size() * sizeof(TerrainVertex))
        );
        gpu->vertex_buffer = bgfx::createVertexBuffer(vb_mem, s_terrain_vertex_layout);

        if (!bgfx::isValid(gpu->vertex_buffer)) {
            log(LogLevel::Error, "Failed to create terrain vertex buffer");
        }
    }

    // Create static index buffer (fallback)
    if (!m_indices.empty()) {
        const bgfx::Memory* ib_mem = bgfx::copy(
            m_indices.data(),
            static_cast<uint32_t>(m_indices.size() * sizeof(uint32_t))
        );
        gpu->index_buffer = bgfx::createIndexBuffer(ib_mem, BGFX_BUFFER_INDEX32);

        if (!bgfx::isValid(gpu->index_buffer)) {
            log(LogLevel::Error, "Failed to create terrain index buffer");
        }
    }

    // Create per-chunk dynamic index buffers for LOD
    uint32_t max_indices_per_chunk = (m_settings.chunk_resolution - 1) * (m_settings.chunk_resolution - 1) * 6;
    gpu->chunk_index_buffers.resize(m_chunks.size());
    for (size_t i = 0; i < m_chunks.size(); ++i) {
        gpu->chunk_index_buffers[i] = bgfx::createDynamicIndexBuffer(
            max_indices_per_chunk,
            BGFX_BUFFER_INDEX32 | BGFX_BUFFER_ALLOW_RESIZE
        );
    }

    // Create splat map texture if available
    if (m_splat_map && m_splat_map->is_valid()) {
        uint32_t width = m_splat_map->get_width();
        uint32_t height = m_splat_map->get_height();
        const std::vector<float>& float_data = m_splat_map->get_data();
        std::vector<uint8_t> byte_data;
        byte_data.reserve(float_data.size());
        
        for (float val : float_data) {
            byte_data.push_back(static_cast<uint8_t>(std::clamp(val * 255.0f, 0.0f, 255.0f)));
        }

        const bgfx::Memory* tex_mem = bgfx::copy(byte_data.data(), static_cast<uint32_t>(byte_data.size()));
        gpu->splat_texture = bgfx::createTexture2D(
            static_cast<uint16_t>(width),
            static_cast<uint16_t>(height),
            false, 1,
            bgfx::TextureFormat::RGBA8,
            BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
            tex_mem
        );
    }

    // Create uniform handles
    gpu->u_terrainParams = bgfx::createUniform("u_terrainParams", bgfx::UniformType::Vec4);
    gpu->u_layer0Params = bgfx::createUniform("u_layer0Params", bgfx::UniformType::Vec4);
    gpu->u_layer1Params = bgfx::createUniform("u_layer1Params", bgfx::UniformType::Vec4);
    gpu->u_layer2Params = bgfx::createUniform("u_layer2Params", bgfx::UniformType::Vec4);
    gpu->u_layer3Params = bgfx::createUniform("u_layer3Params", bgfx::UniformType::Vec4);

    // Create sampler handles
    gpu->s_splatMap = bgfx::createUniform("s_splatMap", bgfx::UniformType::Sampler);
    gpu->s_layer0Albedo = bgfx::createUniform("s_layer0Albedo", bgfx::UniformType::Sampler);
    gpu->s_layer0Normal = bgfx::createUniform("s_layer0Normal", bgfx::UniformType::Sampler);
    gpu->s_layer0ARM = bgfx::createUniform("s_layer0ARM", bgfx::UniformType::Sampler);
    gpu->s_layer1Albedo = bgfx::createUniform("s_layer1Albedo", bgfx::UniformType::Sampler);
    gpu->s_layer1Normal = bgfx::createUniform("s_layer1Normal", bgfx::UniformType::Sampler);
    gpu->s_layer1ARM = bgfx::createUniform("s_layer1ARM", bgfx::UniformType::Sampler);
    gpu->s_layer2Albedo = bgfx::createUniform("s_layer2Albedo", bgfx::UniformType::Sampler);
    gpu->s_layer2Normal = bgfx::createUniform("s_layer2Normal", bgfx::UniformType::Sampler);
    gpu->s_layer2ARM = bgfx::createUniform("s_layer2ARM", bgfx::UniformType::Sampler);
    gpu->s_layer3Albedo = bgfx::createUniform("s_layer3Albedo", bgfx::UniformType::Sampler);
    gpu->s_layer3Normal = bgfx::createUniform("s_layer3Normal", bgfx::UniformType::Sampler);
    gpu->s_layer3ARM = bgfx::createUniform("s_layer3ARM", bgfx::UniformType::Sampler);

    // Load terrain shaders
    bgfx::ShaderHandle vs = load_terrain_shader("vs_terrain");
    bgfx::ShaderHandle fs = load_terrain_shader("fs_terrain");

    if (bgfx::isValid(vs) && bgfx::isValid(fs)) {
        gpu->program = bgfx::createProgram(vs, fs, true);
        log(LogLevel::Info, "Terrain shader program loaded");
    } else {
        log(LogLevel::Warn, "Terrain shader not available, terrain will not render");
    }

    // Load shadow shaders (use shared shadow shader if available)
    bgfx::ShaderHandle shadow_vs = load_terrain_shader("vs_terrain");
    bgfx::ShaderHandle shadow_fs = load_terrain_shader("fs_shadow");

    if (bgfx::isValid(shadow_vs) && bgfx::isValid(shadow_fs)) {
        gpu->shadow_program = bgfx::createProgram(shadow_vs, shadow_fs, true);
    }

    s_gpu_resources[this] = std::move(gpu);

    log(LogLevel::Info, "Terrain GPU resources created");
}

void TerrainRenderer::destroy_gpu_resources() {
    auto it = s_gpu_resources.find(this);
    if (it == s_gpu_resources.end()) return;

    TerrainGPUResources* gpu = it->second.get();

    // Destroy buffers
    if (bgfx::isValid(gpu->vertex_buffer)) bgfx::destroy(gpu->vertex_buffer);
    if (bgfx::isValid(gpu->index_buffer)) bgfx::destroy(gpu->index_buffer);
    if (bgfx::isValid(gpu->splat_texture)) bgfx::destroy(gpu->splat_texture);

    // Destroy programs
    if (bgfx::isValid(gpu->program)) bgfx::destroy(gpu->program);
    if (bgfx::isValid(gpu->shadow_program)) bgfx::destroy(gpu->shadow_program);

    // Destroy chunk index buffers
    for (auto& dib : gpu->chunk_index_buffers) {
        if (bgfx::isValid(dib)) bgfx::destroy(dib);
    }

    // Destroy uniforms
    if (bgfx::isValid(gpu->u_terrainParams)) bgfx::destroy(gpu->u_terrainParams);
    if (bgfx::isValid(gpu->u_layer0Params)) bgfx::destroy(gpu->u_layer0Params);
    if (bgfx::isValid(gpu->u_layer1Params)) bgfx::destroy(gpu->u_layer1Params);
    if (bgfx::isValid(gpu->u_layer2Params)) bgfx::destroy(gpu->u_layer2Params);
    if (bgfx::isValid(gpu->u_layer3Params)) bgfx::destroy(gpu->u_layer3Params);

    // Destroy samplers
    if (bgfx::isValid(gpu->s_splatMap)) bgfx::destroy(gpu->s_splatMap);
    if (bgfx::isValid(gpu->s_layer0Albedo)) bgfx::destroy(gpu->s_layer0Albedo);
    if (bgfx::isValid(gpu->s_layer0Normal)) bgfx::destroy(gpu->s_layer0Normal);
    if (bgfx::isValid(gpu->s_layer0ARM)) bgfx::destroy(gpu->s_layer0ARM);
    if (bgfx::isValid(gpu->s_layer1Albedo)) bgfx::destroy(gpu->s_layer1Albedo);
    if (bgfx::isValid(gpu->s_layer1Normal)) bgfx::destroy(gpu->s_layer1Normal);
    if (bgfx::isValid(gpu->s_layer1ARM)) bgfx::destroy(gpu->s_layer1ARM);
    if (bgfx::isValid(gpu->s_layer2Albedo)) bgfx::destroy(gpu->s_layer2Albedo);
    if (bgfx::isValid(gpu->s_layer2Normal)) bgfx::destroy(gpu->s_layer2Normal);
    if (bgfx::isValid(gpu->s_layer2ARM)) bgfx::destroy(gpu->s_layer2ARM);
    if (bgfx::isValid(gpu->s_layer3Albedo)) bgfx::destroy(gpu->s_layer3Albedo);
    if (bgfx::isValid(gpu->s_layer3Normal)) bgfx::destroy(gpu->s_layer3Normal);
    if (bgfx::isValid(gpu->s_layer3ARM)) bgfx::destroy(gpu->s_layer3ARM);

    s_gpu_resources.erase(it);

    log(LogLevel::Info, "Terrain GPU resources destroyed");
}

void TerrainRenderer::upload_chunk_data(const TerrainChunk& chunk) {
    auto it = s_gpu_resources.find(this);
    if (it == s_gpu_resources.end()) return;

    TerrainGPUResources* gpu = it->second.get();
    if (!bgfx::isValid(gpu->vertex_buffer)) return;

    // Calculate vertex range for this chunk
    uint32_t chunk_vertex_count = m_settings.chunk_resolution * m_settings.chunk_resolution;
    uint32_t chunk_idx = chunk.grid_z * m_settings.chunks_per_side + chunk.grid_x;
    uint32_t vertex_offset = chunk_idx * chunk_vertex_count;

    if (vertex_offset + chunk_vertex_count > m_vertices.size()) return;

    // For full chunk rebuild, we destroy and recreate the entire vertex buffer
    // This is simpler than partial updates and sufficient for editor use
    const bgfx::Memory* vb_mem = bgfx::copy(
        m_vertices.data(),
        static_cast<uint32_t>(m_vertices.size() * sizeof(TerrainVertex))
    );

    // Destroy old buffer and create new one
    if (bgfx::isValid(gpu->vertex_buffer)) {
        bgfx::destroy(gpu->vertex_buffer);
    }

    init_vertex_layout();
    gpu->vertex_buffer = bgfx::createVertexBuffer(vb_mem, s_terrain_vertex_layout);
}

void TerrainRenderer::rebuild_dirty_region(const AABB& dirty_region) {
    if (!m_initialized || !m_heightmap || !m_heightmap->is_valid()) return;

    bool any_rebuilt = false;

    // Find and rebuild chunks that overlap the dirty region
    for (auto& chunk : m_chunks) {
        // Check if chunk bounds intersect dirty region
        bool intersects =
            chunk.bounds.min.x <= dirty_region.max.x &&
            chunk.bounds.max.x >= dirty_region.min.x &&
            chunk.bounds.min.z <= dirty_region.max.z &&
            chunk.bounds.max.z >= dirty_region.min.z;

        if (intersects) {
            // Regenerate vertices for this chunk
            regenerate_chunk_vertices(chunk);
            any_rebuilt = true;
        }
    }

    // If any chunks were rebuilt, upload to GPU
    if (any_rebuilt) {
        auto it = s_gpu_resources.find(this);
        if (it != s_gpu_resources.end()) {
            TerrainGPUResources* gpu = it->second.get();

            // Recreate vertex buffer with updated data
            if (!m_vertices.empty()) {
                if (bgfx::isValid(gpu->vertex_buffer)) {
                    bgfx::destroy(gpu->vertex_buffer);
                }

                const bgfx::Memory* vb_mem = bgfx::copy(
                    m_vertices.data(),
                    static_cast<uint32_t>(m_vertices.size() * sizeof(TerrainVertex))
                );
                gpu->vertex_buffer = bgfx::createVertexBuffer(vb_mem, s_terrain_vertex_layout);
            }
        }
    }
}

void TerrainRenderer::update_splat_texture() {
    if (!m_splat_map || !m_splat_map->is_valid()) return;

    auto it = s_gpu_resources.find(this);
    if (it == s_gpu_resources.end()) return;

    TerrainGPUResources* gpu = it->second.get();

    // Destroy old splat texture
    if (bgfx::isValid(gpu->splat_texture)) {
        bgfx::destroy(gpu->splat_texture);
    }

    // Create new texture from splat map data
    uint32_t width = m_splat_map->get_width();
    uint32_t height = m_splat_map->get_height();

    // Convert float data to uint8 RGBA
    const auto& float_data = m_splat_map->get_data();
    std::vector<uint8_t> rgba_data(width * height * 4);

    for (uint32_t i = 0; i < width * height; ++i) {
        for (uint32_t c = 0; c < 4; ++c) {
            uint32_t src_idx = i * 4 + c;
            float val = (src_idx < float_data.size()) ? float_data[src_idx] : 0.0f;
            rgba_data[i * 4 + c] = static_cast<uint8_t>(std::clamp(val * 255.0f, 0.0f, 255.0f));
        }
    }

    const bgfx::Memory* tex_mem = bgfx::copy(rgba_data.data(), static_cast<uint32_t>(rgba_data.size()));
    gpu->splat_texture = bgfx::createTexture2D(
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height),
        false, 1,
        bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
        tex_mem
    );
}

void TerrainRenderer::regenerate_chunk_vertices(TerrainChunk& chunk) {
    uint32_t chunks_per_side = m_settings.chunks_per_side;
    uint32_t chunk_resolution = m_settings.chunk_resolution;

    float chunk_size_x = m_terrain_scale.x / chunks_per_side;
    float chunk_size_z = m_terrain_scale.z / chunks_per_side;

    float min_x = chunk.grid_x * chunk_size_x;
    float min_z = chunk.grid_z * chunk_size_z;

    uint32_t chunk_idx = chunk.grid_z * chunks_per_side + chunk.grid_x;
    uint32_t vertex_offset = chunk_idx * chunk_resolution * chunk_resolution;

    // Reset height bounds
    chunk.bounds.min.y = std::numeric_limits<float>::max();
    chunk.bounds.max.y = std::numeric_limits<float>::lowest();

    for (uint32_t z = 0; z < chunk_resolution; ++z) {
        for (uint32_t x = 0; x < chunk_resolution; ++x) {
            float local_u = static_cast<float>(x) / (chunk_resolution - 1);
            float local_v = static_cast<float>(z) / (chunk_resolution - 1);

            float world_x = min_x + local_u * chunk_size_x;
            float world_z = min_z + local_v * chunk_size_z;

            float global_u = world_x / m_terrain_scale.x;
            float global_v = world_z / m_terrain_scale.z;

            float height = m_heightmap->sample(global_u, global_v) * m_terrain_scale.y;

            uint32_t vi = vertex_offset + z * chunk_resolution + x;
            if (vi >= m_vertices.size()) continue;

            TerrainVertex& vertex = m_vertices[vi];
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

            // Update chunk height bounds
            chunk.bounds.min.y = std::min(chunk.bounds.min.y, height);
            chunk.bounds.max.y = std::max(chunk.bounds.max.y, height);
        }
    }
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
    out_indices.clear();

    uint32_t step = 1u << lod;  // Our step size (2^lod)
    uint32_t last = resolution - 1;

    // Calculate neighbor step sizes
    uint32_t north_step = 1u << std::max(north_lod, lod);
    uint32_t south_step = 1u << std::max(south_lod, lod);
    uint32_t east_step = 1u << std::max(east_lod, lod);
    uint32_t west_step = 1u << std::max(west_lod, lod);

    out_indices.reserve((resolution / step) * (resolution / step) * 6);

    // Generate interior triangles (not touching any edge)
    for (uint32_t z = step; z < last - step; z += step) {
        for (uint32_t x = step; x < last - step; x += step) {
            uint32_t i00 = z * resolution + x;
            uint32_t i10 = i00 + step;
            uint32_t i01 = i00 + step * resolution;
            uint32_t i11 = i01 + step;

            out_indices.push_back(i00);
            out_indices.push_back(i01);
            out_indices.push_back(i10);

            out_indices.push_back(i10);
            out_indices.push_back(i01);
            out_indices.push_back(i11);
        }
    }

    // Generate north edge (z = 0)
    if (north_lod > lod) {
        // Neighbor is coarser - need to create fan triangles
        for (uint32_t x = 0; x < last; x += north_step) {
            uint32_t base_x = x;
            uint32_t next_x = std::min(x + north_step, last);

            // Top edge vertex (on the edge)
            uint32_t i_edge_left = base_x;
            uint32_t i_edge_right = next_x;

            // Interior vertices at our LOD level (one row down)
            uint32_t i_interior = step * resolution + base_x + north_step / 2;

            // Create fan from coarse edge to fine interior
            for (uint32_t fx = base_x; fx < next_x; fx += step) {
                uint32_t i0 = fx;
                uint32_t i1 = std::min(fx + step, next_x);
                uint32_t i2 = step * resolution + fx;
                uint32_t i3 = step * resolution + std::min(fx + step, next_x);

                if (fx == base_x) {
                    // First triangle connects to left edge point
                    out_indices.push_back(i0);
                    out_indices.push_back(i2);
                    out_indices.push_back(i3);
                } else if (fx + step >= next_x) {
                    // Last triangle connects to right edge point
                    out_indices.push_back(i_edge_right);
                    out_indices.push_back(i2);
                    out_indices.push_back(i3);
                } else {
                    // Interior triangles
                    out_indices.push_back(i_edge_left);
                    out_indices.push_back(i2);
                    out_indices.push_back(i3);
                }
            }
        }
    } else {
        // Same or finer - normal triangulation along edge
        for (uint32_t x = 0; x < last; x += step) {
            uint32_t i00 = x;
            uint32_t i10 = x + step;
            uint32_t i01 = step * resolution + x;
            uint32_t i11 = step * resolution + x + step;

            out_indices.push_back(i00);
            out_indices.push_back(i01);
            out_indices.push_back(i10);

            out_indices.push_back(i10);
            out_indices.push_back(i01);
            out_indices.push_back(i11);
        }
    }

    // Generate south edge (z = last - step)
    if (south_lod > lod) {
        uint32_t z = last - step;
        for (uint32_t x = 0; x < last; x += south_step) {
            uint32_t next_x = std::min(x + south_step, last);

            for (uint32_t fx = x; fx < next_x; fx += step) {
                uint32_t i00 = z * resolution + fx;
                uint32_t i10 = z * resolution + std::min(fx + step, next_x);
                uint32_t i01 = last * resolution + x;
                uint32_t i11 = last * resolution + next_x;

                out_indices.push_back(i00);
                out_indices.push_back(i01);
                out_indices.push_back(i10);

                if (fx + step < next_x) {
                    out_indices.push_back(i10);
                    out_indices.push_back(i01);
                    out_indices.push_back(i11);
                }
            }
        }
    } else {
        uint32_t z = last - step;
        for (uint32_t x = 0; x < last; x += step) {
            uint32_t i00 = z * resolution + x;
            uint32_t i10 = z * resolution + x + step;
            uint32_t i01 = last * resolution + x;
            uint32_t i11 = last * resolution + x + step;

            out_indices.push_back(i00);
            out_indices.push_back(i01);
            out_indices.push_back(i10);

            out_indices.push_back(i10);
            out_indices.push_back(i01);
            out_indices.push_back(i11);
        }
    }

    // Generate west edge (x = 0)
    if (west_lod > lod) {
        for (uint32_t z = step; z < last - step; z += west_step) {
            uint32_t next_z = std::min(z + west_step, last - step);

            for (uint32_t fz = z; fz < next_z; fz += step) {
                uint32_t i00 = fz * resolution;
                uint32_t i10 = fz * resolution + step;
                uint32_t i01 = std::min(fz + step, next_z) * resolution;
                uint32_t i11 = std::min(fz + step, next_z) * resolution + step;

                out_indices.push_back(i00);
                out_indices.push_back(i01);
                out_indices.push_back(i10);

                out_indices.push_back(i10);
                out_indices.push_back(i01);
                out_indices.push_back(i11);
            }
        }
    } else {
        for (uint32_t z = step; z < last - step; z += step) {
            uint32_t i00 = z * resolution;
            uint32_t i10 = z * resolution + step;
            uint32_t i01 = (z + step) * resolution;
            uint32_t i11 = (z + step) * resolution + step;

            out_indices.push_back(i00);
            out_indices.push_back(i01);
            out_indices.push_back(i10);

            out_indices.push_back(i10);
            out_indices.push_back(i01);
            out_indices.push_back(i11);
        }
    }

    // Generate east edge (x = last - step)
    if (east_lod > lod) {
        uint32_t x = last - step;
        for (uint32_t z = step; z < last - step; z += east_step) {
            uint32_t next_z = std::min(z + east_step, last - step);

            for (uint32_t fz = z; fz < next_z; fz += step) {
                uint32_t i00 = fz * resolution + x;
                uint32_t i10 = fz * resolution + last;
                uint32_t i01 = std::min(fz + step, next_z) * resolution + x;
                uint32_t i11 = std::min(fz + step, next_z) * resolution + last;

                out_indices.push_back(i00);
                out_indices.push_back(i01);
                out_indices.push_back(i10);

                out_indices.push_back(i10);
                out_indices.push_back(i01);
                out_indices.push_back(i11);
            }
        }
    } else {
        uint32_t x = last - step;
        for (uint32_t z = step; z < last - step; z += step) {
            uint32_t i00 = z * resolution + x;
            uint32_t i10 = z * resolution + last;
            uint32_t i01 = (z + step) * resolution + x;
            uint32_t i11 = (z + step) * resolution + last;

            out_indices.push_back(i00);
            out_indices.push_back(i01);
            out_indices.push_back(i10);

            out_indices.push_back(i10);
            out_indices.push_back(i01);
            out_indices.push_back(i11);
        }
    }
}

void TerrainIndexGenerator::pregenerate_all_lods(uint32_t base_resolution, uint32_t num_lods,
                                                  std::vector<std::vector<uint32_t>>& out_index_buffers) {
    out_index_buffers.resize(num_lods);

    for (uint32_t lod = 0; lod < num_lods; ++lod) {
        generate_lod_indices(base_resolution, lod, out_index_buffers[lod]);
    }
}

// Streaming implementation

void TerrainRenderer::enable_streaming(const TerrainStreamingConfig& config) {
    if (m_streamer) {
        m_streamer->shutdown();
    }

    m_streamer = std::make_unique<TerrainStreamer>();
    m_streamer->init(config, m_terrain_bounds, m_heightmap);

    log(LogLevel::Info, "Terrain streaming enabled");
}

void TerrainRenderer::disable_streaming() {
    if (m_streamer) {
        m_streamer->shutdown();
        m_streamer.reset();
        log(LogLevel::Info, "Terrain streaming disabled");
    }
}

// TerrainQuadtree implementation

void TerrainQuadtree::build(const AABB& terrain_bounds, uint32_t max_depth) {
    m_max_depth = max_depth;

    m_root = std::make_unique<QuadtreeNode>();
    m_root->bounds = terrain_bounds;
    m_root->depth = 0;
    m_root->lod = max_depth;  // Coarsest LOD at root
    m_root->is_leaf = true;
    m_root->chunk_index = 0;
}

void TerrainQuadtree::update(const Vec3& camera_pos, float lod_distance) {
    if (!m_root) return;

    // Reset all nodes to leaves first
    reset_to_leaves(m_root.get());

    // Subdivide based on camera distance
    subdivide(m_root.get(), camera_pos, lod_distance);

    // Assign chunk indices to leaves
    uint32_t chunk_index = 0;
    assign_chunk_indices(m_root.get(), chunk_index);
}

void TerrainQuadtree::subdivide(QuadtreeNode* node, const Vec3& camera_pos, float lod_distance) {
    if (!node || node->depth >= m_max_depth) return;

    Vec3 center = node->bounds.center();
    // Use XZ distance (horizontal distance to camera)
    float distance = std::sqrt(
        (center.x - camera_pos.x) * (center.x - camera_pos.x) +
        (center.z - camera_pos.z) * (center.z - camera_pos.z)
    );

    // Calculate threshold for this depth level
    // Higher depth = closer to camera = finer detail
    float threshold = lod_distance * std::pow(2.0f, static_cast<float>(m_max_depth - node->depth - 1));

    if (distance < threshold) {
        // Subdivide into 4 children
        node->is_leaf = false;

        Vec3 min = node->bounds.min;
        Vec3 max = node->bounds.max;
        Vec3 mid = center;

        // NW (0), NE (1), SW (2), SE (3)
        AABB child_bounds[4] = {
            AABB(Vec3(min.x, min.y, min.z), Vec3(mid.x, max.y, mid.z)),  // NW
            AABB(Vec3(mid.x, min.y, min.z), Vec3(max.x, max.y, mid.z)),  // NE
            AABB(Vec3(min.x, min.y, mid.z), Vec3(mid.x, max.y, max.z)),  // SW
            AABB(Vec3(mid.x, min.y, mid.z), Vec3(max.x, max.y, max.z))   // SE
        };

        for (int i = 0; i < 4; ++i) {
            node->children[i] = std::make_unique<QuadtreeNode>();
            node->children[i]->bounds = child_bounds[i];
            node->children[i]->depth = node->depth + 1;
            node->children[i]->lod = m_max_depth - node->depth - 1;
            node->children[i]->is_leaf = true;

            // Recursively subdivide children
            subdivide(node->children[i].get(), camera_pos, lod_distance);
        }
    }
}

void TerrainQuadtree::get_visible_chunks(const Frustum& frustum,
                                          std::vector<uint32_t>& out_chunks) const {
    out_chunks.clear();
    if (m_root) {
        collect_visible(m_root.get(), frustum, out_chunks);
    }
}

void TerrainQuadtree::collect_visible(const QuadtreeNode* node, const Frustum& frustum,
                                       std::vector<uint32_t>& out_chunks) const {
    if (!node) return;

    // Frustum culling
    if (!frustum.contains_aabb(node->bounds)) {
        return;
    }

    if (node->is_leaf) {
        if (node->chunk_index != UINT32_MAX) {
            out_chunks.push_back(node->chunk_index);
        }
    } else {
        for (int i = 0; i < 4; ++i) {
            if (node->children[i]) {
                collect_visible(node->children[i].get(), frustum, out_chunks);
            }
        }
    }
}

void TerrainQuadtree::get_leaves(std::vector<const QuadtreeNode*>& out_leaves) const {
    out_leaves.clear();
    if (m_root) {
        collect_leaves(m_root.get(), out_leaves);
    }
}

void TerrainQuadtree::collect_leaves(const QuadtreeNode* node,
                                      std::vector<const QuadtreeNode*>& out_leaves) const {
    if (!node) return;

    if (node->is_leaf) {
        out_leaves.push_back(node);
    } else {
        for (int i = 0; i < 4; ++i) {
            if (node->children[i]) {
                collect_leaves(node->children[i].get(), out_leaves);
            }
        }
    }
}

void TerrainQuadtree::reset_to_leaves(QuadtreeNode* node) {
    if (!node) return;

    if (node->has_children()) {
        for (int i = 0; i < 4; ++i) {
            node->children[i].reset();
        }
    }
    node->is_leaf = true;
    node->chunk_index = UINT32_MAX;
}

void TerrainQuadtree::assign_chunk_indices(QuadtreeNode* node, uint32_t& chunk_index) {
    if (!node) return;

    if (node->is_leaf) {
        node->chunk_index = chunk_index++;
    } else {
        for (int i = 0; i < 4; ++i) {
            if (node->children[i]) {
                assign_chunk_indices(node->children[i].get(), chunk_index);
            }
        }
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
