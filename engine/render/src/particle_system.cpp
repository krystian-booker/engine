#include <engine/render/particle_system.hpp>
#include <engine/render/renderer.hpp>
#include <engine/core/log.hpp>
#include <algorithm>
#include <random>
#include <cmath>
#include <fstream>

#ifdef Warn
#undef Warn
#endif

namespace engine::render {

// Shader loading helpers
static bgfx::ShaderHandle load_particle_shader(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return BGFX_INVALID_HANDLE;

    std::streampos pos = file.tellg();
    if (pos <= 0) return BGFX_INVALID_HANDLE;

    auto size = static_cast<std::streamsize>(pos);
    file.seekg(0, std::ios::beg);

    const bgfx::Memory* mem = bgfx::alloc(static_cast<uint32_t>(size) + 1);
    file.read(reinterpret_cast<char*>(mem->data), size);
    if (file.gcount() != size) return BGFX_INVALID_HANDLE;

    mem->data[size] = '\0';
    return bgfx::createShader(mem);
}

static std::string get_particle_shader_path() {
    auto type = bgfx::getRendererType();
    switch (type) {
        case bgfx::RendererType::Direct3D11:
        case bgfx::RendererType::Direct3D12: return "shaders/dx11/";
        case bgfx::RendererType::Vulkan: return "shaders/spirv/";
        case bgfx::RendererType::OpenGL: return "shaders/glsl/";
        default: return "shaders/spirv/";
    }
}

using namespace engine::core;

// Random number generator for particle emission
static thread_local std::mt19937 s_rng{std::random_device{}()};

static float random_float(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(s_rng);
}

static Vec3 random_vec3(const Vec3& min, const Vec3& max) {
    return Vec3{
        random_float(min.x, max.x),
        random_float(min.y, max.y),
        random_float(min.z, max.z)
    };
}

static Vec3 random_on_unit_sphere() {
    float theta = random_float(0.0f, 2.0f * 3.14159265f);
    float phi = std::acos(random_float(-1.0f, 1.0f));
    float sin_phi = std::sin(phi);
    return Vec3{
        sin_phi * std::cos(theta),
        sin_phi * std::sin(theta),
        std::cos(phi)
    };
}

static Vec3 random_in_unit_sphere() {
    Vec3 p = random_on_unit_sphere();
    float r = std::cbrt(random_float(0.0f, 1.0f));
    return p * r;
}

// ============================================================================
// ColorGradient
// ============================================================================

Vec4 ColorGradient::evaluate(float t) const {
    if (keys.empty()) return Vec4{1.0f};
    if (keys.size() == 1) return keys[0].color;

    t = glm::clamp(t, 0.0f, 1.0f);

    // Find the two keys to interpolate between
    for (size_t i = 0; i < keys.size() - 1; ++i) {
        if (t >= keys[i].time && t <= keys[i + 1].time) {
            float local_t = (t - keys[i].time) / (keys[i + 1].time - keys[i].time);
            return glm::mix(keys[i].color, keys[i + 1].color, local_t);
        }
    }

    return keys.back().color;
}

// ============================================================================
// Curve
// ============================================================================

float Curve::evaluate(float t) const {
    if (keys.empty()) return 1.0f;
    if (keys.size() == 1) return keys[0].value;

    t = glm::clamp(t, 0.0f, 1.0f);

    // Find the two keys to interpolate between
    for (size_t i = 0; i < keys.size() - 1; ++i) {
        if (t >= keys[i].time && t <= keys[i + 1].time) {
            float local_t = (t - keys[i].time) / (keys[i + 1].time - keys[i].time);
            return glm::mix(keys[i].value, keys[i + 1].value, local_t);
        }
    }

    return keys.back().value;
}

// ============================================================================
// ParticleSystem
// ============================================================================

ParticleSystem::~ParticleSystem() {
    shutdown();
}

void ParticleSystem::init(IRenderer* renderer) {
    if (m_initialized) return;

    m_renderer = renderer;

    // Create vertex layout for particle billboards
    m_vertex_layout
        .begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord1, 4, bgfx::AttribType::Float)  // rotation, size, life, flags
        .end();

    // Create uniforms
    m_u_particle_params = bgfx::createUniform("u_particleParams", bgfx::UniformType::Vec4, 4);
    m_u_camera_pos = bgfx::createUniform("u_cameraPos", bgfx::UniformType::Vec4);
    m_s_texture = bgfx::createUniform("s_texture", bgfx::UniformType::Sampler);
    m_s_depth = bgfx::createUniform("s_depth", bgfx::UniformType::Sampler);

    // Load particle shader program
    std::string path = get_particle_shader_path();
    bgfx::ShaderHandle vs = load_particle_shader(path + "vs_particle.sc.bin");
    bgfx::ShaderHandle fs = load_particle_shader(path + "fs_particle.sc.bin");

    if (bgfx::isValid(vs) && bgfx::isValid(fs)) {
        m_particle_program = bgfx::createProgram(vs, fs, true);
        log(LogLevel::Info, "Particle shaders loaded successfully");
    } else {
        log(LogLevel::Warn, "Failed to load particle shaders, particles will not render");
        if (bgfx::isValid(vs)) bgfx::destroy(vs);
        if (bgfx::isValid(fs)) bgfx::destroy(fs);
    }

    m_initialized = true;
    log(LogLevel::Info, "Particle system initialized");
}

void ParticleSystem::shutdown() {
    if (!m_initialized) return;

    // Destroy all emitter runtimes
    for (auto* runtime : m_active_emitters) {
        destroy_emitter_runtime(runtime);
    }
    m_active_emitters.clear();

    // Destroy uniforms
    if (bgfx::isValid(m_u_particle_params)) {
        bgfx::destroy(m_u_particle_params);
        m_u_particle_params = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_u_camera_pos)) {
        bgfx::destroy(m_u_camera_pos);
        m_u_camera_pos = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_s_texture)) {
        bgfx::destroy(m_s_texture);
        m_s_texture = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_s_depth)) {
        bgfx::destroy(m_s_depth);
        m_s_depth = BGFX_INVALID_HANDLE;
    }

    // Destroy shader program
    if (bgfx::isValid(m_particle_program)) {
        bgfx::destroy(m_particle_program);
        m_particle_program = BGFX_INVALID_HANDLE;
    }

    m_initialized = false;
    log(LogLevel::Info, "Particle system shut down");
}

ParticleEmitterRuntime* ParticleSystem::create_emitter_runtime(const ParticleEmitterConfig& config) {
    auto* runtime = new ParticleEmitterRuntime();

    // Reserve particle storage
    runtime->particles.reserve(config.max_particles);

    // Create GPU buffers
    uint32_t vertex_count = config.max_particles * 4;  // 4 vertices per particle (quad)
    uint32_t index_count = config.max_particles * 6;   // 6 indices per particle (2 triangles)

    runtime->vertex_buffer = bgfx::createDynamicVertexBuffer(
        vertex_count,
        m_vertex_layout,
        BGFX_BUFFER_ALLOW_RESIZE
    );

    runtime->index_buffer = bgfx::createDynamicIndexBuffer(
        index_count,
        BGFX_BUFFER_ALLOW_RESIZE | BGFX_BUFFER_INDEX32
    );

    runtime->initialized = true;
    m_active_emitters.push_back(runtime);

    return runtime;
}

void ParticleSystem::destroy_emitter_runtime(ParticleEmitterRuntime* runtime) {
    if (!runtime) return;

    // Remove from active list
    auto it = std::find(m_active_emitters.begin(), m_active_emitters.end(), runtime);
    if (it != m_active_emitters.end()) {
        m_active_emitters.erase(it);
    }

    // Destroy GPU resources
    if (bgfx::isValid(runtime->vertex_buffer)) {
        bgfx::destroy(runtime->vertex_buffer);
    }
    if (bgfx::isValid(runtime->index_buffer)) {
        bgfx::destroy(runtime->index_buffer);
    }

    delete runtime;
}

void ParticleSystem::update(float dt) {
    // Update is handled per-emitter in update_emitter()
}

void ParticleSystem::update_emitter(ParticleEmitterRuntime* runtime,
                                     const ParticleEmitterConfig& config,
                                     const Mat4& transform,
                                     float dt) {
    if (!runtime || !runtime->initialized || !runtime->playing) return;
    if (!config.enabled) return;

    runtime->elapsed_time += dt;

    // Emit new particles
    if (config.emission_rate > 0.0f) {
        runtime->emit_accumulator += dt * config.emission_rate;
        uint32_t particles_to_emit = static_cast<uint32_t>(runtime->emit_accumulator);
        runtime->emit_accumulator -= static_cast<float>(particles_to_emit);

        if (particles_to_emit > 0) {
            emit_particles(runtime, config, transform, particles_to_emit);
        }
    }

    // Simulate particles
    simulate_particles(runtime, config, dt);

    // Upload to GPU
    upload_particles(runtime);
}

void ParticleSystem::render(const CameraData& camera) {
    // Render is handled per-emitter in render_emitter()
}

void ParticleSystem::render_emitter(const ParticleEmitterRuntime* runtime,
                                     const ParticleEmitterConfig& config,
                                     const CameraData& camera) {
    if (!runtime || runtime->alive_count == 0) return;
    if (!bgfx::isValid(m_particle_program)) return;

    // Set camera position uniform
    float camera_pos[4] = {camera.position.x, camera.position.y, camera.position.z, 0.0f};
    bgfx::setUniform(m_u_camera_pos, camera_pos);

    // Set particle parameters
    float params[16] = {
        config.soft_particles ? 1.0f : 0.0f,
        config.soft_particle_distance,
        0.0f, 0.0f,
        // Additional params as needed
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f
    };
    bgfx::setUniform(m_u_particle_params, params, 4);

    // Set texture
    if (config.texture.valid()) {
        uint16_t tex_idx = m_renderer->get_native_texture_handle(config.texture);
        if (tex_idx != bgfx::kInvalidHandle) {
            bgfx::TextureHandle tex_handle = { tex_idx };
            bgfx::setTexture(0, m_s_texture, tex_handle);
        }
    }

    // Set buffers
    bgfx::setVertexBuffer(0, runtime->vertex_buffer);
    bgfx::setIndexBuffer(runtime->index_buffer, 0, runtime->alive_count * 6);

    // Set render state based on blend mode
    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LESS;

    switch (config.blend_mode) {
        case ParticleBlendMode::Alpha:
            state |= BGFX_STATE_BLEND_ALPHA;
            break;
        case ParticleBlendMode::Additive:
            state |= BGFX_STATE_BLEND_ADD;
            break;
        case ParticleBlendMode::Multiply:
            state |= BGFX_STATE_BLEND_MULTIPLY;
            break;
        case ParticleBlendMode::Premultiplied:
            state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA);
            break;
    }

    bgfx::setState(state);

    // Submit draw call to transparent pass
    uint16_t view_id = static_cast<uint16_t>(RenderView::MainTransparent);
    bgfx::submit(view_id, m_particle_program);
}

void ParticleSystem::emit_burst(ParticleEmitterRuntime* runtime,
                                 const ParticleEmitterConfig& config,
                                 const Mat4& transform,
                                 uint32_t count) {
    if (!runtime || !runtime->initialized) return;
    emit_particles(runtime, config, transform, count);
    upload_particles(runtime);
}

void ParticleSystem::play(ParticleEmitterRuntime* runtime) {
    if (runtime) runtime->playing = true;
}

void ParticleSystem::pause(ParticleEmitterRuntime* runtime) {
    if (runtime) runtime->playing = false;
}

void ParticleSystem::stop(ParticleEmitterRuntime* runtime) {
    if (runtime) {
        runtime->playing = false;
        runtime->particles.clear();
        runtime->alive_count = 0;
    }
}

void ParticleSystem::reset(ParticleEmitterRuntime* runtime) {
    if (runtime) {
        runtime->particles.clear();
        runtime->alive_count = 0;
        runtime->emit_accumulator = 0.0f;
        runtime->elapsed_time = 0.0f;
        runtime->playing = true;
    }
}

uint32_t ParticleSystem::get_total_particle_count() const {
    uint32_t total = 0;
    for (const auto* runtime : m_active_emitters) {
        if (runtime) total += runtime->alive_count;
    }
    return total;
}

uint32_t ParticleSystem::get_active_emitter_count() const {
    return static_cast<uint32_t>(m_active_emitters.size());
}

void ParticleSystem::emit_particles(ParticleEmitterRuntime* runtime,
                                     const ParticleEmitterConfig& config,
                                     const Mat4& transform,
                                     uint32_t count) {
    uint32_t available = config.max_particles - static_cast<uint32_t>(runtime->particles.size());
    count = std::min(count, available);

    for (uint32_t i = 0; i < count; ++i) {
        ParticleGPU particle;

        // Position
        Vec3 pos = generate_emission_position(config.emission_shape, transform);
        float lifetime = config.lifetime + random_float(-config.lifetime_variance, config.lifetime_variance);
        lifetime = std::max(0.01f, lifetime);
        particle.position_life = Vec4{pos, lifetime};

        // Velocity
        Vec3 vel = generate_emission_velocity(config.emission_shape, config.initial_velocity, config.velocity_variance);
        if (config.world_space) {
            vel = Vec3(transform * Vec4(vel, 0.0f));
        }
        float size = config.initial_size + random_float(-config.size_variance, config.size_variance);
        size = std::max(0.001f, size);
        particle.velocity_size = Vec4{vel, size};

        // Color (starts at gradient t=0)
        particle.color = config.color_over_life.evaluate(0.0f);

        // Rotation and params
        float rot = config.initial_rotation + random_float(-config.rotation_variance, config.rotation_variance);
        float ang_vel = config.angular_velocity + random_float(-config.angular_velocity_variance, config.angular_velocity_variance);
        particle.params = Vec4{rot, ang_vel, lifetime, 0.0f};

        runtime->particles.push_back(particle);
    }

    runtime->alive_count = static_cast<uint32_t>(runtime->particles.size());
}

void ParticleSystem::simulate_particles(ParticleEmitterRuntime* runtime,
                                         const ParticleEmitterConfig& config,
                                         float dt) {
    auto& particles = runtime->particles;

    // Process particles, removing dead ones
    auto it = particles.begin();
    while (it != particles.end()) {
        ParticleGPU& p = *it;

        // Decrease life
        p.position_life.w -= dt;

        // Remove dead particles
        if (p.position_life.w <= 0.0f) {
            it = particles.erase(it);
            continue;
        }

        // Calculate life ratio (0 = just born, 1 = about to die)
        float initial_life = p.params.z;
        float life_ratio = 1.0f - (p.position_life.w / initial_life);
        life_ratio = glm::clamp(life_ratio, 0.0f, 1.0f);

        // Apply forces
        Vec3 velocity{p.velocity_size.x, p.velocity_size.y, p.velocity_size.z};
        velocity += config.gravity * dt;

        // Apply drag
        if (config.drag > 0.0f) {
            velocity *= 1.0f - (config.drag * dt);
        }

        // Apply speed over life curve
        float speed_mult = config.speed_over_life.evaluate(life_ratio);
        Vec3 scaled_velocity = velocity * speed_mult;

        // Update position
        Vec3 position{p.position_life.x, p.position_life.y, p.position_life.z};
        position += scaled_velocity * dt;
        p.position_life = Vec4{position, p.position_life.w};

        // Store velocity (unscaled for next frame)
        p.velocity_size = Vec4{velocity, p.velocity_size.w};

        // Update size over life
        float base_size = p.velocity_size.w;  // This is initial size, should store separately
        float size_mult = config.size_over_life.evaluate(life_ratio);
        // Note: For proper implementation, store initial size in params

        // Update color over life
        p.color = config.color_over_life.evaluate(life_ratio);

        // Update rotation
        float rotation = p.params.x;
        float angular_vel = p.params.y;
        rotation += angular_vel * dt;
        p.params.x = rotation;

        ++it;
    }

    runtime->alive_count = static_cast<uint32_t>(particles.size());
}

void ParticleSystem::upload_particles(ParticleEmitterRuntime* runtime) {
    if (runtime->alive_count == 0) return;

    // Build vertex and index data for billboards
    struct ParticleVertex {
        float x, y, z;      // Position
        float u, v;         // UV
        float r, g, b, a;   // Color
        float rot, size, life, flags;  // Extra data
    };

    std::vector<ParticleVertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(runtime->alive_count * 4);
    indices.reserve(runtime->alive_count * 6);

    uint32_t vertex_offset = 0;
    for (const auto& p : runtime->particles) {
        Vec3 pos{p.position_life.x, p.position_life.y, p.position_life.z};
        Vec4 color = p.color;
        float size = p.velocity_size.w;
        float rotation = p.params.x;
        float life_ratio = 1.0f - (p.position_life.w / p.params.z);

        // Create 4 vertices for quad (billboard expansion done in vertex shader)
        ParticleVertex v;
        v.r = color.r; v.g = color.g; v.b = color.b; v.a = color.a;
        v.rot = rotation; v.size = size; v.life = life_ratio; v.flags = 0.0f;

        // Bottom-left
        v.x = pos.x; v.y = pos.y; v.z = pos.z;
        v.u = 0.0f; v.v = 0.0f;
        vertices.push_back(v);

        // Bottom-right
        v.u = 1.0f; v.v = 0.0f;
        vertices.push_back(v);

        // Top-right
        v.u = 1.0f; v.v = 1.0f;
        vertices.push_back(v);

        // Top-left
        v.u = 0.0f; v.v = 1.0f;
        vertices.push_back(v);

        // Indices for two triangles
        indices.push_back(vertex_offset + 0);
        indices.push_back(vertex_offset + 1);
        indices.push_back(vertex_offset + 2);
        indices.push_back(vertex_offset + 0);
        indices.push_back(vertex_offset + 2);
        indices.push_back(vertex_offset + 3);

        vertex_offset += 4;
    }

    // Update GPU buffers
    if (!vertices.empty()) {
        const bgfx::Memory* vertex_mem = bgfx::copy(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(ParticleVertex)));
        bgfx::update(runtime->vertex_buffer, 0, vertex_mem);
    }

    if (!indices.empty()) {
        const bgfx::Memory* index_mem = bgfx::copy(indices.data(), static_cast<uint32_t>(indices.size() * sizeof(uint32_t)));
        bgfx::update(runtime->index_buffer, 0, index_mem);
    }
}

Vec3 ParticleSystem::generate_emission_position(const EmissionShapeConfig& shape, const Mat4& transform) {
    Vec3 local_pos{0.0f};

    switch (shape.shape) {
        case EmissionShape::Point:
            local_pos = Vec3{0.0f};
            break;

        case EmissionShape::Sphere:
            if (shape.emit_from_surface) {
                local_pos = random_on_unit_sphere() * shape.size.x;
            } else {
                local_pos = random_in_unit_sphere() * shape.size.x;
            }
            break;

        case EmissionShape::Box:
            local_pos = random_vec3(-shape.size, shape.size);
            if (shape.emit_from_surface) {
                // Project to nearest face
                int axis = static_cast<int>(random_float(0.0f, 3.0f));
                axis = std::min(axis, 2);
                local_pos[axis] = (random_float(0.0f, 1.0f) > 0.5f) ? shape.size[axis] : -shape.size[axis];
            }
            break;

        case EmissionShape::Cone:
            // Emit within cone angle
            {
                float angle_rad = glm::radians(shape.angle);
                float theta = random_float(0.0f, 2.0f * 3.14159265f);
                float phi = random_float(0.0f, angle_rad);
                float r = random_float(0.0f, shape.size.x);
                local_pos = Vec3{
                    r * std::sin(phi) * std::cos(theta),
                    r * std::cos(phi),
                    r * std::sin(phi) * std::sin(theta)
                };
            }
            break;

        case EmissionShape::Circle:
            {
                float angle = random_float(0.0f, 2.0f * 3.14159265f);
                float r = shape.emit_from_surface ? shape.size.x : random_float(0.0f, shape.size.x);
                local_pos = Vec3{r * std::cos(angle), 0.0f, r * std::sin(angle)};
            }
            break;

        case EmissionShape::Hemisphere:
            {
                Vec3 p = random_on_unit_sphere();
                if (p.y < 0.0f) p.y = -p.y;
                if (!shape.emit_from_surface) {
                    p *= std::cbrt(random_float(0.0f, 1.0f));
                }
                local_pos = p * shape.size.x;
            }
            break;
    }

    // Transform to world space
    return Vec3(transform * Vec4(local_pos, 1.0f));
}

Vec3 ParticleSystem::generate_emission_velocity(const EmissionShapeConfig& shape,
                                                  const Vec3& base_velocity,
                                                  const Vec3& variance) {
    Vec3 vel = base_velocity;
    vel += random_vec3(-variance, variance);
    return vel;
}

} // namespace engine::render
