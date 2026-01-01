#pragma once

#include <engine/render/types.hpp>
#include <engine/render/render_pipeline.hpp>
#include <engine/core/math.hpp>
#include <bgfx/bgfx.h>
#include <vector>
#include <memory>
#include <cstdint>

namespace engine::render {

using namespace engine::core;

// ============================================================================
// Particle GPU Data (48 bytes, GPU-aligned)
// ============================================================================

struct alignas(16) ParticleGPU {
    Vec4 position_life;      // xyz = position, w = remaining life (seconds)
    Vec4 velocity_size;      // xyz = velocity, w = current size
    Vec4 color;              // rgba
    Vec4 params;             // x = rotation, y = angular_velocity, z = initial_life, w = flags
};

// ============================================================================
// Emission Shape Configuration
// ============================================================================

enum class EmissionShape : uint8_t {
    Point,      // Emit from a single point
    Sphere,     // Emit from sphere surface or volume
    Box,        // Emit from box surface or volume
    Cone,       // Emit in a cone direction
    Circle,     // Emit from circle edge or area
    Hemisphere  // Emit from hemisphere surface
};

struct EmissionShapeConfig {
    EmissionShape shape = EmissionShape::Point;
    Vec3 size{1.0f};           // Shape dimensions (radius for sphere, half-extents for box)
    float angle = 30.0f;       // Cone angle in degrees
    bool emit_from_surface = false;  // true = surface only, false = volume
};

// ============================================================================
// Particle Blend Modes
// ============================================================================

enum class ParticleBlendMode : uint8_t {
    Alpha,      // Standard alpha blending
    Additive,   // Additive blending (good for fire, glow)
    Multiply,   // Multiply blending
    Premultiplied  // Premultiplied alpha
};

// ============================================================================
// Simple Gradient (color over lifetime)
// ============================================================================

struct ColorKey {
    Vec4 color{1.0f};
    float time = 0.0f;  // 0.0 to 1.0
};

struct ColorGradient {
    std::vector<ColorKey> keys;

    ColorGradient() {
        // Default: white to white
        keys.push_back({Vec4{1.0f}, 0.0f});
        keys.push_back({Vec4{1.0f}, 1.0f});
    }

    Vec4 evaluate(float t) const;
};

// ============================================================================
// Simple Curve (size/speed over lifetime)
// ============================================================================

struct CurveKey {
    float value = 1.0f;
    float time = 0.0f;  // 0.0 to 1.0
};

struct Curve {
    std::vector<CurveKey> keys;

    Curve() {
        // Default: constant 1.0
        keys.push_back({1.0f, 0.0f});
        keys.push_back({1.0f, 1.0f});
    }

    float evaluate(float t) const;
};

// ============================================================================
// Particle Emitter Configuration
// ============================================================================

struct ParticleEmitterConfig {
    // Emission
    uint32_t max_particles = 10000;
    float emission_rate = 100.0f;       // Particles per second
    EmissionShapeConfig emission_shape;

    // Initial particle properties
    float lifetime = 2.0f;
    float lifetime_variance = 0.5f;     // Random variance (+/-)
    Vec3 initial_velocity{0.0f, 5.0f, 0.0f};
    Vec3 velocity_variance{1.0f};       // Random variance (+/-)
    float initial_size = 0.1f;
    float size_variance = 0.02f;
    float initial_rotation = 0.0f;      // Radians
    float rotation_variance = 3.14159f; // Random variance
    float angular_velocity = 0.0f;      // Radians per second
    float angular_velocity_variance = 1.0f;

    // Over lifetime modifiers
    ColorGradient color_over_life;
    Curve size_over_life;
    Curve speed_over_life;

    // Forces
    Vec3 gravity{0.0f, -9.81f, 0.0f};
    float drag = 0.0f;                  // Air resistance (0-1)

    // Rendering
    TextureHandle texture;
    ParticleBlendMode blend_mode = ParticleBlendMode::Additive;
    bool face_camera = true;            // Billboard mode
    bool soft_particles = true;         // Fade at depth intersections
    float soft_particle_distance = 0.5f;

    // Behavior
    bool enabled = true;
    bool loop = true;
    bool prewarm = false;               // Simulate on start
    float prewarm_time = 2.0f;
    bool world_space = true;            // Particles in world space vs local

    // Sorting
    bool sort_by_depth = false;         // For proper transparency
};

// ============================================================================
// Particle Emitter Runtime State
// ============================================================================

struct ParticleEmitterRuntime {
    // GPU resources
    bgfx::DynamicVertexBufferHandle vertex_buffer = BGFX_INVALID_HANDLE;
    bgfx::DynamicIndexBufferHandle index_buffer = BGFX_INVALID_HANDLE;

    // State
    uint32_t alive_count = 0;
    float emit_accumulator = 0.0f;
    float elapsed_time = 0.0f;
    bool initialized = false;
    bool playing = true;

    // CPU particle data (for CPU simulation fallback)
    std::vector<ParticleGPU> particles;
};

// ============================================================================
// Particle System
// ============================================================================

class ParticleSystem {
public:
    ParticleSystem() = default;
    ~ParticleSystem();

    // Initialization
    void init(IRenderer* renderer);
    void shutdown();

    // Create/destroy emitter runtime
    ParticleEmitterRuntime* create_emitter_runtime(const ParticleEmitterConfig& config);
    void destroy_emitter_runtime(ParticleEmitterRuntime* runtime);

    // Update all particles (call in FixedUpdate or Update)
    void update(float dt);

    // Update a single emitter
    void update_emitter(ParticleEmitterRuntime* runtime,
                        const ParticleEmitterConfig& config,
                        const Mat4& transform,
                        float dt);

    // Render all active emitters
    void render(const CameraData& camera);

    // Render a single emitter
    void render_emitter(const ParticleEmitterRuntime* runtime,
                        const ParticleEmitterConfig& config,
                        const CameraData& camera);

    // Burst emission
    void emit_burst(ParticleEmitterRuntime* runtime,
                    const ParticleEmitterConfig& config,
                    const Mat4& transform,
                    uint32_t count);

    // Control
    void play(ParticleEmitterRuntime* runtime);
    void pause(ParticleEmitterRuntime* runtime);
    void stop(ParticleEmitterRuntime* runtime);
    void reset(ParticleEmitterRuntime* runtime);

    // Statistics
    uint32_t get_total_particle_count() const;
    uint32_t get_active_emitter_count() const;

private:
    // Internal methods
    void emit_particles(ParticleEmitterRuntime* runtime,
                        const ParticleEmitterConfig& config,
                        const Mat4& transform,
                        uint32_t count);
    void simulate_particles(ParticleEmitterRuntime* runtime,
                            const ParticleEmitterConfig& config,
                            float dt);
    void upload_particles(ParticleEmitterRuntime* runtime);

    Vec3 generate_emission_position(const EmissionShapeConfig& shape, const Mat4& transform);
    Vec3 generate_emission_velocity(const EmissionShapeConfig& shape,
                                     const Vec3& base_velocity,
                                     const Vec3& variance);

    IRenderer* m_renderer = nullptr;
    bool m_initialized = false;

    // Shader programs
    bgfx::ProgramHandle m_particle_program = BGFX_INVALID_HANDLE;

    // Uniforms
    bgfx::UniformHandle m_u_particle_params = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_camera_pos = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_s_texture = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_s_depth = BGFX_INVALID_HANDLE;

    // Vertex layout
    bgfx::VertexLayout m_vertex_layout;

    // Active emitters (for update/render)
    std::vector<ParticleEmitterRuntime*> m_active_emitters;

    // Default white texture
    TextureHandle m_default_texture;
};

} // namespace engine::render
