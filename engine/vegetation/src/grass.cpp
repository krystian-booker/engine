#include <engine/vegetation/grass.hpp>
#include <engine/core/log.hpp>
#include <bgfx/bgfx.h>
#include <algorithm>
#include <random>
#include <cmath>
#include <fstream>

namespace engine::vegetation {

using namespace engine::core;

// Grass instance data layout for GPU (must match shader)
struct GrassInstanceGPU {
    float position[3];      // 12 bytes - xyz position
    float rotation;         // 4 bytes - Y rotation
    float scale;            // 4 bytes - scale
    float bend;             // 4 bytes - bend amount
    uint32_t color_packed;  // 4 bytes - RGBA8 color
    float random;           // 4 bytes - random value
    // Total: 32 bytes
};
static_assert(sizeof(GrassInstanceGPU) == 32, "GrassInstanceGPU must be 32 bytes");

// Grass blade vertex
struct GrassVertex {
    float x, y, z;          // Position
    float u, v;             // Texcoord
    float height_factor;    // 0 at base, 1 at tip (for wind animation)
};

// Shader path helper
static std::string get_shader_path() {
    // Platform-specific shader binary path
#if defined(_WIN32)
    return "shaders/dx11/";
#elif defined(__APPLE__)
    return "shaders/metal/";
#else
    return "shaders/glsl/";
#endif
}

// Load shader from file
static bgfx::ShaderHandle load_shader(const std::string& name) {
    std::string path = get_shader_path() + name + ".bin";
    std::ifstream file(path, std::ios::binary | std::ios::ate);

    if (!file.is_open()) {
        log(LogLevel::Warn, ("Grass shader not found: " + path).c_str());
        return BGFX_INVALID_HANDLE;
    }

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    const bgfx::Memory* mem = bgfx::alloc(static_cast<uint32_t>(size) + 1);
    file.read(reinterpret_cast<char*>(mem->data), size);
    mem->data[size] = '\0';

    return bgfx::createShader(mem);
}

// GPU resources (shared across all grass instances)
static bgfx::VertexLayout s_grass_vertex_layout;
static bgfx::VertexLayout s_grass_instance_layout;
static bgfx::VertexBufferHandle s_grass_blade_vb = BGFX_INVALID_HANDLE;
static bgfx::IndexBufferHandle s_grass_blade_ib = BGFX_INVALID_HANDLE;
static bgfx::ProgramHandle s_grass_program = BGFX_INVALID_HANDLE;
static bgfx::ProgramHandle s_grass_shadow_program = BGFX_INVALID_HANDLE;
static bgfx::UniformHandle s_u_grass_wind = BGFX_INVALID_HANDLE;
static bgfx::UniformHandle s_u_grass_params = BGFX_INVALID_HANDLE;
static bgfx::UniformHandle s_s_grass_texture = BGFX_INVALID_HANDLE;
static bgfx::UniformHandle s_s_noise_texture = BGFX_INVALID_HANDLE;
static uint32_t s_grass_ref_count = 0;

// Per-chunk instance buffer storage
struct GrassChunkGPU {
    bgfx::InstanceDataBuffer instance_buffer;
    bool buffer_valid = false;
};
static std::vector<GrassChunkGPU> s_chunk_gpu_data;

// Global instance
static GrassSystem* s_grass_system = nullptr;

GrassSystem& get_grass_system() {
    if (!s_grass_system) {
        static GrassSystem instance;
        s_grass_system = &instance;
    }
    return *s_grass_system;
}

GrassSystem::~GrassSystem() {
    shutdown();
}

void GrassSystem::init(const AABB& terrain_bounds, const GrassSettings& settings) {
    if (m_initialized) shutdown();

    m_terrain_bounds = terrain_bounds;
    m_settings = settings;

    // Calculate chunk grid
    float terrain_width = terrain_bounds.max.x - terrain_bounds.min.x;
    float terrain_depth = terrain_bounds.max.z - terrain_bounds.min.z;

    m_chunks_x = static_cast<uint32_t>(std::ceil(terrain_width / settings.chunk_size));
    m_chunks_z = static_cast<uint32_t>(std::ceil(terrain_depth / settings.chunk_size));

    m_chunks.resize(m_chunks_x * m_chunks_z);

    // Initialize chunks
    for (uint32_t z = 0; z < m_chunks_z; ++z) {
        for (uint32_t x = 0; x < m_chunks_x; ++x) {
            uint32_t idx = z * m_chunks_x + x;
            GrassChunk& chunk = m_chunks[idx];

            chunk.position.x = terrain_bounds.min.x + x * settings.chunk_size;
            chunk.position.y = terrain_bounds.min.z + z * settings.chunk_size;
            chunk.size = settings.chunk_size;

            chunk.bounds.min = Vec3(chunk.position.x, terrain_bounds.min.y, chunk.position.y);
            chunk.bounds.max = Vec3(chunk.position.x + chunk.size,
                                     terrain_bounds.max.y,
                                     chunk.position.y + chunk.size);
        }
    }

    create_gpu_resources();
    m_initialized = true;
}

void GrassSystem::shutdown() {
    if (!m_initialized) return;

    destroy_gpu_resources();
    m_chunks.clear();
    m_interactors.clear();

    m_initialized = false;
}

void GrassSystem::set_settings(const GrassSettings& settings) {
    m_settings = settings;
}

void GrassSystem::generate_grass(std::function<float(float x, float z)> height_func,
                                  std::function<float(float x, float z)> density_func,
                                  std::function<Vec3(float x, float z)> normal_func) {
    if (!m_initialized) return;

    m_stats.total_instances = 0;

    for (auto& chunk : m_chunks) {
        generate_chunk(chunk, height_func, density_func, normal_func);
        m_stats.total_instances += static_cast<uint32_t>(chunk.instances.size());
    }
}

void GrassSystem::generate_from_density_map(const void* density_data, uint32_t width, uint32_t height,
                                             std::function<float(float x, float z)> height_func) {
    if (!density_data) return;

    const uint8_t* data = static_cast<const uint8_t*>(density_data);

    auto density_func = [=](float x, float z) -> float {
        float terrain_width = m_terrain_bounds.max.x - m_terrain_bounds.min.x;
        float terrain_depth = m_terrain_bounds.max.z - m_terrain_bounds.min.z;

        float u = (x - m_terrain_bounds.min.x) / terrain_width;
        float v = (z - m_terrain_bounds.min.z) / terrain_depth;

        u = std::clamp(u, 0.0f, 1.0f);
        v = std::clamp(v, 0.0f, 1.0f);

        uint32_t px = static_cast<uint32_t>(u * (width - 1));
        uint32_t py = static_cast<uint32_t>(v * (height - 1));

        return data[py * width + px] / 255.0f;
    };

    generate_grass(height_func, density_func, nullptr);
}

void GrassSystem::regenerate_region(const AABB& region) {
    // Find affected chunks and regenerate
    for (auto& chunk : m_chunks) {
        if (chunk.bounds.intersects(region)) {
            chunk.dirty = true;
        }
    }
}

void GrassSystem::clear() {
    for (auto& chunk : m_chunks) {
        chunk.instances.clear();
        chunk.dirty = true;
    }
    m_stats.total_instances = 0;
}

void GrassSystem::update(float dt, const Vec3& camera_position, const Frustum& frustum) {
    if (!m_initialized) return;

    update_wind(dt);
    update_interactions(dt);
    update_chunk_visibility(camera_position, frustum);
}

void GrassSystem::add_interactor(const GrassInteractor& interactor) {
    m_interactors.push_back(interactor);
}

void GrassSystem::remove_interactor(uint32_t index) {
    if (index < m_interactors.size()) {
        m_interactors.erase(m_interactors.begin() + index);
    }
}

void GrassSystem::clear_interactors() {
    m_interactors.clear();
}

void GrassSystem::set_player_position(const Vec3& position, const Vec3& velocity) {
    m_player_position = position;
    m_player_velocity = velocity;
}

void GrassSystem::render(uint16_t view_id) {
    if (!m_initialized) return;

    // Check if shader is available
    if (!bgfx::isValid(s_grass_program)) {
        return;
    }

    m_stats.visible_instances = 0;
    m_stats.visible_chunks = 0;

    // Set wind uniform (shared across all chunks)
    bgfx::setUniform(s_u_grass_wind, &m_wind_params);

    // Set grass params uniform
    Vec4 grass_params(
        m_settings.blade_width,
        m_settings.blade_height,
        m_settings.alpha_cutoff,
        m_settings.fade_start_distance
    );
    bgfx::setUniform(s_u_grass_params, &grass_params);

    // Set textures if available
    if (m_blade_texture != UINT32_MAX) {
        bgfx::setTexture(0, s_s_grass_texture, bgfx::TextureHandle{static_cast<uint16_t>(m_blade_texture)});
    }
    if (m_noise_texture != UINT32_MAX) {
        bgfx::setTexture(1, s_s_noise_texture, bgfx::TextureHandle{static_cast<uint16_t>(m_noise_texture)});
    }

    // Render state
    uint64_t state = BGFX_STATE_WRITE_RGB
                   | BGFX_STATE_WRITE_A
                   | BGFX_STATE_WRITE_Z
                   | BGFX_STATE_DEPTH_TEST_LESS
                   | BGFX_STATE_CULL_CW
                   | BGFX_STATE_MSAA;

    if (m_settings.use_alpha_cutoff) {
        state |= BGFX_STATE_BLEND_ALPHA;
    }

    for (size_t i = 0; i < m_chunks.size(); ++i) {
        const GrassChunk& chunk = m_chunks[i];
        if (!chunk.visible || chunk.instances.empty()) continue;

        // Get GPU data for this chunk
        if (i >= s_chunk_gpu_data.size()) continue;
        const GrassChunkGPU& gpu = s_chunk_gpu_data[i];
        if (!gpu.buffer_valid) continue;

        // Set vertex and index buffers
        bgfx::setVertexBuffer(0, s_grass_blade_vb);
        bgfx::setIndexBuffer(s_grass_blade_ib);

        // Set instance data buffer
        bgfx::setInstanceDataBuffer(&gpu.instance_buffer);

        // Set state
        bgfx::setState(state);

        // Submit
        bgfx::submit(view_id, s_grass_program);

        m_stats.visible_instances += static_cast<uint32_t>(chunk.instances.size());
        m_stats.visible_chunks++;
    }
}

void GrassSystem::render_shadow(uint16_t view_id) {
    if (!m_initialized || !m_settings.cast_shadows) return;

    // Check if shadow shader is available
    if (!bgfx::isValid(s_grass_shadow_program)) {
        return;
    }

    // Render state for shadow pass
    uint64_t state = BGFX_STATE_WRITE_Z
                   | BGFX_STATE_DEPTH_TEST_LESS
                   | BGFX_STATE_CULL_CW;

    // Set wind uniform (grass still needs to animate in shadows)
    bgfx::setUniform(s_u_grass_wind, &m_wind_params);

    for (size_t i = 0; i < m_chunks.size(); ++i) {
        const GrassChunk& chunk = m_chunks[i];
        if (!chunk.visible || chunk.instances.empty()) continue;

        if (i >= s_chunk_gpu_data.size()) continue;
        const GrassChunkGPU& gpu = s_chunk_gpu_data[i];
        if (!gpu.buffer_valid) continue;

        bgfx::setVertexBuffer(0, s_grass_blade_vb);
        bgfx::setIndexBuffer(s_grass_blade_ib);
        bgfx::setInstanceDataBuffer(&gpu.instance_buffer);
        bgfx::setState(state);
        bgfx::submit(view_id, s_grass_shadow_program);
    }
}

void GrassSystem::generate_chunk(GrassChunk& chunk,
                                  std::function<float(float x, float z)> height_func,
                                  std::function<float(float x, float z)> density_func,
                                  std::function<Vec3(float x, float z)> normal_func) {
    chunk.instances.clear();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> pos_dist(0.0f, 1.0f);
    std::uniform_real_distribution<float> angle_dist(0.0f, 6.28318f);

    float base_density = m_settings.density;
    float spacing = 1.0f / std::sqrt(base_density);

    float chunk_min_x = chunk.position.x;
    float chunk_min_z = chunk.position.y;
    float chunk_max_x = chunk_min_x + chunk.size;
    float chunk_max_z = chunk_min_z + chunk.size;

    // Jittered grid sampling
    for (float z = chunk_min_z; z < chunk_max_z; z += spacing) {
        for (float x = chunk_min_x; x < chunk_max_x; x += spacing) {
            // Jitter position
            float jx = x + (pos_dist(gen) - 0.5f) * spacing;
            float jz = z + (pos_dist(gen) - 0.5f) * spacing;

            // Check density
            float local_density = 1.0f;
            if (density_func) {
                local_density = density_func(jx, jz);
            }

            // Density-based rejection
            if (pos_dist(gen) > local_density * (1.0f - m_settings.density_variance + pos_dist(gen) * m_settings.density_variance * 2.0f)) {
                continue;
            }

            // Get height
            float y = height_func ? height_func(jx, jz) : 0.0f;

            // Check max instances
            if (chunk.instances.size() >= m_settings.max_instances / m_chunks.size()) {
                break;
            }

            GrassInstance instance;
            instance.position = Vec3(jx, y, jz);
            instance.rotation = angle_dist(gen);
            instance.scale = m_settings.blade_height *
                             (1.0f - m_settings.blade_height_variance + pos_dist(gen) * m_settings.blade_height_variance * 2.0f);
            instance.bend = 0.0f;
            instance.random = pos_dist(gen);

            // Calculate color variation
            Vec3 color = m_settings.base_color;
            float color_var = m_settings.color_variance * (pos_dist(gen) * 2.0f - 1.0f);
            color = color + Vec3(color_var);

            // Mix in dry color
            if (m_settings.dry_amount > 0.0f && pos_dist(gen) < m_settings.dry_amount) {
                float dry_blend = pos_dist(gen);
                color = color * (1.0f - dry_blend) + m_settings.dry_color * dry_blend;
            }

            color = clamp(color, Vec3(0.0f), Vec3(1.0f));

            // Pack color
            uint8_t r = static_cast<uint8_t>(color.x * 255.0f);
            uint8_t g = static_cast<uint8_t>(color.y * 255.0f);
            uint8_t b = static_cast<uint8_t>(color.z * 255.0f);
            instance.color_packed = (255u << 24) | (b << 16) | (g << 8) | r;

            chunk.instances.push_back(instance);

            // Update chunk bounds
            chunk.bounds.min.y = std::min(chunk.bounds.min.y, y);
            chunk.bounds.max.y = std::max(chunk.bounds.max.y, y + instance.scale);
        }
    }

    chunk.dirty = true;
}

void GrassSystem::update_chunk_visibility(const Vec3& camera_pos, const Frustum& frustum) {
    m_stats.total_chunks = static_cast<uint32_t>(m_chunks.size());

    for (auto& chunk : m_chunks) {
        // Distance check
        Vec3 chunk_center = (chunk.bounds.min + chunk.bounds.max) * 0.5f;
        chunk.distance_to_camera = length(chunk_center - camera_pos);

        if (chunk.distance_to_camera > m_settings.cull_distance) {
            chunk.visible = false;
            continue;
        }

        // Frustum check
        chunk.visible = frustum.contains_aabb(chunk.bounds);

        // LOD level
        if (chunk.distance_to_camera < m_settings.lod_start_distance) {
            chunk.lod = 0;
        } else if (chunk.distance_to_camera < m_settings.lod_end_distance) {
            float t = (chunk.distance_to_camera - m_settings.lod_start_distance) /
                      (m_settings.lod_end_distance - m_settings.lod_start_distance);
            chunk.lod = static_cast<uint32_t>(t * 2.0f);
        } else {
            chunk.lod = 2;
        }

        // Upload if dirty and visible
        if (chunk.visible && chunk.dirty) {
            upload_chunk(chunk);
        }
    }
}

void GrassSystem::update_wind(float dt) {
    m_wind_time += dt * m_settings.wind.speed;

    m_wind_params.x = m_settings.wind.direction.x;
    m_wind_params.y = m_settings.wind.direction.y;
    m_wind_params.z = m_wind_time;
    m_wind_params.w = m_settings.wind.strength;
}

void GrassSystem::update_interactions(float dt) {
    if (!m_settings.enable_interaction) return;

    // Update grass bend based on player and interactors
    for (auto& chunk : m_chunks) {
        if (!chunk.visible) continue;

        for (auto& instance : chunk.instances) {
            float target_bend = 0.0f;

            // Player interaction
            Vec3 to_player = instance.position - m_player_position;
            to_player.y = 0.0f;
            float dist_to_player = length(to_player);

            if (dist_to_player < m_settings.interaction_radius) {
                float strength = 1.0f - (dist_to_player / m_settings.interaction_radius);
                strength *= m_settings.interaction_strength;
                target_bend = std::max(target_bend, strength);
            }

            // Other interactors
            for (const auto& interactor : m_interactors) {
                Vec3 to_interactor = instance.position - interactor.position;
                to_interactor.y = 0.0f;
                float dist = length(to_interactor);

                if (dist < interactor.radius) {
                    float strength = (1.0f - dist / interactor.radius) * interactor.strength;
                    target_bend = std::max(target_bend, strength);
                }
            }

            // Smooth blend to target
            float blend_speed = target_bend > instance.bend ?
                                10.0f :  // Fast push down
                                m_settings.interaction_recovery;  // Slow recovery

            instance.bend += (target_bend - instance.bend) * std::min(1.0f, blend_speed * dt);
        }
    }
}

void GrassSystem::upload_chunk(GrassChunk& chunk) {
    if (chunk.instances.empty()) {
        chunk.dirty = false;
        return;
    }

    // Find chunk index
    size_t chunk_idx = &chunk - m_chunks.data();
    if (chunk_idx >= s_chunk_gpu_data.size()) {
        s_chunk_gpu_data.resize(m_chunks.size());
    }

    GrassChunkGPU& gpu = s_chunk_gpu_data[chunk_idx];

    // Check instance buffer availability
    uint32_t num_instances = static_cast<uint32_t>(chunk.instances.size());
    uint16_t stride = sizeof(GrassInstanceGPU);

    if (bgfx::getAvailInstanceDataBuffer(num_instances, stride) < num_instances) {
        // Not enough buffer space, skip this frame
        return;
    }

    // Allocate instance buffer
    bgfx::allocInstanceDataBuffer(&gpu.instance_buffer, num_instances, stride);

    // Copy instance data
    GrassInstanceGPU* dst = reinterpret_cast<GrassInstanceGPU*>(gpu.instance_buffer.data);
    for (size_t i = 0; i < chunk.instances.size(); ++i) {
        const GrassInstance& src = chunk.instances[i];
        dst[i].position[0] = src.position.x;
        dst[i].position[1] = src.position.y;
        dst[i].position[2] = src.position.z;
        dst[i].rotation = src.rotation;
        dst[i].scale = src.scale;
        dst[i].bend = src.bend;
        dst[i].color_packed = src.color_packed;
        dst[i].random = src.random;
    }

    gpu.buffer_valid = true;
    chunk.dirty = false;
}

void GrassSystem::create_gpu_resources() {
    // Only create shared resources once
    if (s_grass_ref_count > 0) {
        s_grass_ref_count++;
        s_chunk_gpu_data.resize(m_chunks.size());
        return;
    }

    // Create vertex layout for grass blade mesh
    s_grass_vertex_layout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord1, 1, bgfx::AttribType::Float)  // height_factor
        .end();

    // Create instance data layout (32 bytes)
    s_grass_instance_layout
        .begin()
        .add(bgfx::Attrib::TexCoord7, 4, bgfx::AttribType::Float)   // position.xyz, rotation
        .add(bgfx::Attrib::TexCoord6, 4, bgfx::AttribType::Float)   // scale, bend, color_packed (as 2 floats), random
        .end();

    // Create grass blade mesh (simple quad, 2 triangles)
    // The blade is oriented along the Y axis with bottom at origin
    GrassVertex blade_vertices[] = {
        // Position            UV        Height
        {-0.5f, 0.0f, 0.0f,   0.0f, 1.0f,  0.0f},  // Bottom left
        { 0.5f, 0.0f, 0.0f,   1.0f, 1.0f,  0.0f},  // Bottom right
        { 0.5f, 1.0f, 0.0f,   1.0f, 0.0f,  1.0f},  // Top right
        {-0.5f, 1.0f, 0.0f,   0.0f, 0.0f,  1.0f},  // Top left
    };

    uint16_t blade_indices[] = {
        0, 1, 2,  // First triangle
        0, 2, 3   // Second triangle
    };

    // Create vertex buffer
    const bgfx::Memory* vb_mem = bgfx::copy(blade_vertices, sizeof(blade_vertices));
    s_grass_blade_vb = bgfx::createVertexBuffer(vb_mem, s_grass_vertex_layout);

    // Create index buffer
    const bgfx::Memory* ib_mem = bgfx::copy(blade_indices, sizeof(blade_indices));
    s_grass_blade_ib = bgfx::createIndexBuffer(ib_mem);

    // Create uniforms
    s_u_grass_wind = bgfx::createUniform("u_grassWind", bgfx::UniformType::Vec4);
    s_u_grass_params = bgfx::createUniform("u_grassParams", bgfx::UniformType::Vec4);
    s_s_grass_texture = bgfx::createUniform("s_grassTexture", bgfx::UniformType::Sampler);
    s_s_noise_texture = bgfx::createUniform("s_noiseTexture", bgfx::UniformType::Sampler);

    // Load shaders
    bgfx::ShaderHandle vs = load_shader("vs_grass.sc");
    bgfx::ShaderHandle fs = load_shader("fs_grass.sc");

    if (bgfx::isValid(vs) && bgfx::isValid(fs)) {
        s_grass_program = bgfx::createProgram(vs, fs, true);
        log(LogLevel::Info, "Grass shader program loaded");
    } else {
        log(LogLevel::Warn, "Grass shader not available, grass will not render");
    }

    // Load shadow shaders (optional)
    bgfx::ShaderHandle shadow_vs = load_shader("vs_grass_shadow.sc");
    bgfx::ShaderHandle shadow_fs = load_shader("fs_shadow.sc");

    if (bgfx::isValid(shadow_vs) && bgfx::isValid(shadow_fs)) {
        s_grass_shadow_program = bgfx::createProgram(shadow_vs, shadow_fs, true);
    }

    s_grass_ref_count++;
    s_chunk_gpu_data.resize(m_chunks.size());

    log(LogLevel::Info, "Grass GPU resources created");
}

void GrassSystem::destroy_gpu_resources() {
    s_chunk_gpu_data.clear();

    // Only destroy shared resources when last user is gone
    if (s_grass_ref_count > 1) {
        s_grass_ref_count--;
        return;
    }

    if (bgfx::isValid(s_grass_blade_vb)) {
        bgfx::destroy(s_grass_blade_vb);
        s_grass_blade_vb = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(s_grass_blade_ib)) {
        bgfx::destroy(s_grass_blade_ib);
        s_grass_blade_ib = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(s_grass_program)) {
        bgfx::destroy(s_grass_program);
        s_grass_program = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(s_grass_shadow_program)) {
        bgfx::destroy(s_grass_shadow_program);
        s_grass_shadow_program = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(s_u_grass_wind)) {
        bgfx::destroy(s_u_grass_wind);
        s_u_grass_wind = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(s_u_grass_params)) {
        bgfx::destroy(s_u_grass_params);
        s_u_grass_params = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(s_s_grass_texture)) {
        bgfx::destroy(s_s_grass_texture);
        s_s_grass_texture = BGFX_INVALID_HANDLE;
    }

    if (bgfx::isValid(s_s_noise_texture)) {
        bgfx::destroy(s_s_noise_texture);
        s_s_noise_texture = BGFX_INVALID_HANDLE;
    }

    s_grass_ref_count = 0;
    log(LogLevel::Info, "Grass GPU resources destroyed");
}

} // namespace engine::vegetation
