#pragma once

#include <engine/core/math.hpp>
#include <bgfx/bgfx.h>
#include <vector>
#include <memory>
#include <functional>

namespace engine::render {

using namespace engine::core;

// Reflection probe type
enum class ReflectionProbeType : uint8_t {
    Baked,      // Pre-computed cubemap
    Realtime,   // Updated each frame
    Custom      // User-provided cubemap
};

// Reflection probe projection mode
enum class ReflectionProjection : uint8_t {
    Infinite,   // Infinite projection (for skybox-like probes)
    Box         // Box projection (for interior spaces)
};

// Reflection probe blend mode
enum class ReflectionBlendMode : uint8_t {
    Override,   // Use highest priority probe only
    Blend,      // Blend between overlapping probes
    Additive    // Add contributions
};

// Reflection probe configuration
struct ReflectionProbeConfig {
    // Capture settings
    uint32_t resolution = 256;            // Cubemap face resolution
    uint32_t mip_levels = 7;              // Mip levels for roughness
    float near_plane = 0.1f;
    float far_plane = 1000.0f;
    bool hdr = true;                      // Use HDR format

    // Update settings (for realtime probes)
    float update_interval = 0.0f;         // Update interval (0 = every frame)
    bool update_one_face_per_frame = true; // Spread update across frames
    uint32_t max_realtime_probes = 4;     // Maximum concurrent realtime probes

    // Filtering settings
    bool prefilter_environment = true;    // Pre-filter for roughness
    float intensity = 1.0f;
};

// Individual reflection probe
struct ReflectionProbe {
    // Transform
    Vec3 position = Vec3(0.0f);
    Vec3 box_min = Vec3(-5.0f);          // Box projection min (local space)
    Vec3 box_max = Vec3(5.0f);            // Box projection max (local space)

    // Properties
    ReflectionProbeType type = ReflectionProbeType::Baked;
    ReflectionProjection projection = ReflectionProjection::Box;
    float importance = 1.0f;              // Blend importance/priority
    float influence_radius = 10.0f;       // Influence sphere radius
    float blend_distance = 1.0f;          // Blend transition distance

    // Intensity and color
    float intensity = 1.0f;
    Vec3 tint = Vec3(1.0f);

    // Cubemap
    bgfx::TextureHandle cubemap = BGFX_INVALID_HANDLE;
    uint32_t resolution = 256;
    uint32_t mip_levels = 7;

    // State
    bool enabled = true;
    bool needs_update = true;
    uint32_t last_update_frame = 0;
    uint32_t next_face_to_update = 0;     // For incremental updates

    // Probe ID
    uint32_t probe_id = 0;

    // Get AABB in world space
    void get_world_bounds(Vec3& out_min, Vec3& out_max) const {
        out_min = position + box_min;
        out_max = position + box_max;
    }

    // Check if point is in influence range
    bool is_in_range(const Vec3& point) const {
        if (!enabled) return false;
        float dist = length(point - position);
        return dist <= influence_radius;
    }

    // Get influence weight at position
    float get_weight_at(const Vec3& point) const {
        if (!enabled) return 0.0f;

        float dist = length(point - position);
        if (dist > influence_radius) return 0.0f;

        float fade_start = influence_radius - blend_distance;
        if (dist < fade_start) return 1.0f;

        return 1.0f - (dist - fade_start) / blend_distance;
    }

    // Box projection intersection
    Vec3 box_project(const Vec3& world_pos, const Vec3& direction) const {
        if (projection == ReflectionProjection::Infinite) {
            return direction;
        }

        Vec3 world_min = position + box_min;
        Vec3 world_max = position + box_max;

        // Ray-box intersection
        Vec3 first_plane = (world_max - world_pos) / direction;
        Vec3 second_plane = (world_min - world_pos) / direction;

        Vec3 furthest = max(first_plane, second_plane);
        float dist = std::min(std::min(furthest.x, furthest.y), furthest.z);

        Vec3 intersection = world_pos + direction * dist;
        return normalize(intersection - position);
    }
};

// Handle type
using ReflectionProbeHandle = uint32_t;
constexpr ReflectionProbeHandle INVALID_REFLECTION_PROBE = UINT32_MAX;

// Probe render callback (for custom rendering)
using ProbeRenderCallback = std::function<void(const Mat4& view, const Mat4& proj, uint32_t face)>;

// Reflection probe system
class ReflectionProbeSystem {
public:
    ReflectionProbeSystem() = default;
    ~ReflectionProbeSystem();

    // Non-copyable
    ReflectionProbeSystem(const ReflectionProbeSystem&) = delete;
    ReflectionProbeSystem& operator=(const ReflectionProbeSystem&) = delete;

    // Initialize/shutdown
    void init(const ReflectionProbeConfig& config = {});
    void shutdown();
    bool is_initialized() const { return m_initialized; }

    // Configuration
    void set_config(const ReflectionProbeConfig& config) { m_config = config; }
    const ReflectionProbeConfig& get_config() const { return m_config; }

    // Probe management
    ReflectionProbeHandle create_probe(const Vec3& position,
                                        ReflectionProbeType type = ReflectionProbeType::Baked);
    void destroy_probe(ReflectionProbeHandle handle);
    ReflectionProbe* get_probe(ReflectionProbeHandle handle);
    const ReflectionProbe* get_probe(ReflectionProbeHandle handle) const;

    // Set custom cubemap for a probe
    void set_probe_cubemap(ReflectionProbeHandle handle, bgfx::TextureHandle cubemap);

    // Baking
    void bake_probe(ReflectionProbeHandle handle, ProbeRenderCallback render_callback);
    void bake_all(ProbeRenderCallback render_callback);

    // Update (call each frame)
    void update(uint32_t frame_number, ProbeRenderCallback render_callback);

    // Query probes
    std::vector<ReflectionProbeHandle> get_probes_at(const Vec3& position) const;
    ReflectionProbeHandle get_dominant_probe(const Vec3& position) const;

    // Sample environment
    Vec3 sample_environment(const Vec3& position,
                             const Vec3& direction,
                             float roughness) const;

    // Get probe data for shader
    struct ProbeShaderData {
        Vec4 position_radius;     // xyz = position, w = radius
        Vec4 box_min;             // xyz = min, w = importance
        Vec4 box_max;             // xyz = max, w = blend_distance
        Vec4 intensity_projection; // x = intensity, y = projection mode, zw = unused
    };
    std::vector<ProbeShaderData> get_shader_data(const Vec3& camera_pos, uint32_t max_probes) const;

    // Bind probes for rendering
    void bind_probes(bgfx::ViewId view_id, const Vec3& camera_pos);

    // Global environment (skybox fallback)
    void set_skybox(bgfx::TextureHandle cubemap) { m_skybox = cubemap; }
    bgfx::TextureHandle get_skybox() const { return m_skybox; }

    // Statistics
    struct Stats {
        uint32_t total_probes = 0;
        uint32_t realtime_probes = 0;
        uint32_t probes_updated_this_frame = 0;
        uint32_t visible_probes = 0;
    };
    Stats get_stats() const { return m_stats; }

private:
    void capture_face(ReflectionProbe& probe, uint32_t face,
                      ProbeRenderCallback render_callback);
    void prefilter_cubemap(ReflectionProbe& probe);
    Mat4 get_face_view_matrix(const Vec3& position, uint32_t face);
    Mat4 get_face_projection_matrix();

    ReflectionProbeConfig m_config;
    bool m_initialized = false;

    // Probes
    std::vector<ReflectionProbe> m_probes;
    std::vector<bool> m_probe_used;
    uint32_t m_next_probe_id = 1;

    // Rendering resources
    bgfx::FrameBufferHandle m_capture_fb = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_capture_color = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_capture_depth = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_prefilter_program = BGFX_INVALID_HANDLE;

    // Skybox fallback
    bgfx::TextureHandle m_skybox = BGFX_INVALID_HANDLE;

    // Uniforms
    bgfx::UniformHandle u_probe_data = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_environment = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_probe_array = BGFX_INVALID_HANDLE;

    // Frame counter
    uint32_t m_frame_number = 0;

    // Stats
    Stats m_stats;
};

// Global reflection probe system
ReflectionProbeSystem& get_reflection_probe_system();

// Reflection probe utilities
namespace ReflectionProbeUtils {

// Cubemap face directions
inline void get_face_vectors(uint32_t face, Vec3& forward, Vec3& up) {
    switch (face) {
        case 0: forward = Vec3(1, 0, 0);  up = Vec3(0, -1, 0); break;  // +X
        case 1: forward = Vec3(-1, 0, 0); up = Vec3(0, -1, 0); break;  // -X
        case 2: forward = Vec3(0, 1, 0);  up = Vec3(0, 0, 1);  break;  // +Y
        case 3: forward = Vec3(0, -1, 0); up = Vec3(0, 0, -1); break;  // -Y
        case 4: forward = Vec3(0, 0, 1);  up = Vec3(0, -1, 0); break;  // +Z
        case 5: forward = Vec3(0, 0, -1); up = Vec3(0, -1, 0); break;  // -Z
    }
}

// Get UV from direction
inline Vec2 direction_to_uv(const Vec3& dir, uint32_t& out_face) {
    Vec3 abs_dir = Vec3(std::abs(dir.x), std::abs(dir.y), std::abs(dir.z));

    float max_axis = std::max({abs_dir.x, abs_dir.y, abs_dir.z});
    Vec2 uv;

    if (max_axis == abs_dir.x) {
        if (dir.x > 0) {
            out_face = 0;  // +X
            uv = Vec2(-dir.z, -dir.y) / abs_dir.x;
        } else {
            out_face = 1;  // -X
            uv = Vec2(dir.z, -dir.y) / abs_dir.x;
        }
    } else if (max_axis == abs_dir.y) {
        if (dir.y > 0) {
            out_face = 2;  // +Y
            uv = Vec2(dir.x, dir.z) / abs_dir.y;
        } else {
            out_face = 3;  // -Y
            uv = Vec2(dir.x, -dir.z) / abs_dir.y;
        }
    } else {
        if (dir.z > 0) {
            out_face = 4;  // +Z
            uv = Vec2(dir.x, -dir.y) / abs_dir.z;
        } else {
            out_face = 5;  // -Z
            uv = Vec2(-dir.x, -dir.y) / abs_dir.z;
        }
    }

    return uv * 0.5f + Vec2(0.5f);
}

// Calculate mip level from roughness
inline float roughness_to_mip(float roughness, uint32_t mip_count) {
    return roughness * static_cast<float>(mip_count - 1);
}

// GGX importance sampling
inline Vec3 importance_sample_ggx(const Vec2& xi, const Vec3& normal, float roughness) {
    float a = roughness * roughness;

    float phi = 2.0f * 3.14159f * xi.x;
    float cos_theta = std::sqrt((1.0f - xi.y) / (1.0f + (a * a - 1.0f) * xi.y));
    float sin_theta = std::sqrt(1.0f - cos_theta * cos_theta);

    Vec3 h;
    h.x = std::cos(phi) * sin_theta;
    h.y = std::sin(phi) * sin_theta;
    h.z = cos_theta;

    Vec3 up = std::abs(normal.z) < 0.999f ? Vec3(0, 0, 1) : Vec3(1, 0, 0);
    Vec3 tangent = normalize(cross(up, normal));
    Vec3 bitangent = cross(normal, tangent);

    return normalize(tangent * h.x + bitangent * h.y + normal * h.z);
}

// Hammersley sequence for sampling
inline Vec2 hammersley(uint32_t i, uint32_t n) {
    uint32_t bits = i;
    bits = (bits << 16) | (bits >> 16);
    bits = ((bits & 0x55555555) << 1) | ((bits & 0xAAAAAAAA) >> 1);
    bits = ((bits & 0x33333333) << 2) | ((bits & 0xCCCCCCCC) >> 2);
    bits = ((bits & 0x0F0F0F0F) << 4) | ((bits & 0xF0F0F0F0) >> 4);
    bits = ((bits & 0x00FF00FF) << 8) | ((bits & 0xFF00FF00) >> 8);

    float radical_inverse = static_cast<float>(bits) * 2.3283064365386963e-10f;
    return Vec2(static_cast<float>(i) / n, radical_inverse);
}

} // namespace ReflectionProbeUtils

// ECS Component for reflection probes
struct ReflectionProbeComponent {
    ReflectionProbeHandle probe_handle = INVALID_REFLECTION_PROBE;
    bool auto_update = false;
};

} // namespace engine::render
