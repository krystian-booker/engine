#pragma once

#include <engine/core/math.hpp>
#include <bgfx/bgfx.h>
#include <vector>

namespace engine::render {

using namespace engine::core;

// SSR quality presets
enum class SSRQuality : uint8_t {
    Low,      // 16 steps, no hi-z, no temporal
    Medium,   // 32 steps, hi-z, no temporal
    High,     // 64 steps, hi-z, temporal
    Ultra     // 128 steps, hi-z, temporal, higher resolution
};

// SSR configuration
struct SSRConfig {
    // Ray marching parameters
    uint32_t max_steps = 64;              // Maximum ray march steps
    float max_distance = 100.0f;          // Maximum ray travel distance
    float thickness = 0.5f;               // Surface thickness for hit detection
    float stride = 1.0f;                  // Initial step stride (pixels)
    float stride_cutoff = 100.0f;         // Distance to start using max stride

    // Hi-Z acceleration
    bool use_hiz = true;                  // Use hierarchical-Z for acceleration
    uint32_t hiz_levels = 6;              // Number of hi-z mip levels

    // Temporal filtering
    bool temporal_enabled = true;         // Enable temporal reprojection
    float temporal_weight = 0.95f;        // Weight of previous frame (0-1)

    // Quality settings
    float resolution_scale = 1.0f;        // Resolution scale (0.5 = half res)
    bool jitter_enabled = true;           // Jitter ray origin for AA
    float roughness_threshold = 0.5f;     // Max roughness to apply SSR

    // Fallback and blending
    float edge_fade_start = 0.9f;         // Start fading at screen edge
    float edge_fade_end = 1.0f;           // Full fade at screen edge
    float intensity = 1.0f;               // Overall SSR intensity
    float fresnel_bias = 0.04f;           // Fresnel effect bias

    // Debug
    bool debug_mode = false;              // Show debug visualization

    // Apply preset
    void apply_preset(SSRQuality quality) {
        switch (quality) {
            case SSRQuality::Low:
                max_steps = 16;
                use_hiz = false;
                temporal_enabled = false;
                resolution_scale = 0.5f;
                break;
            case SSRQuality::Medium:
                max_steps = 32;
                use_hiz = true;
                temporal_enabled = false;
                resolution_scale = 0.75f;
                break;
            case SSRQuality::High:
                max_steps = 64;
                use_hiz = true;
                temporal_enabled = true;
                resolution_scale = 1.0f;
                break;
            case SSRQuality::Ultra:
                max_steps = 128;
                use_hiz = true;
                temporal_enabled = true;
                resolution_scale = 1.0f;
                stride = 0.5f;
                break;
        }
    }
};

// SSR system for screen-space reflections
class SSRSystem {
public:
    SSRSystem() = default;
    ~SSRSystem();

    // Non-copyable
    SSRSystem(const SSRSystem&) = delete;
    SSRSystem& operator=(const SSRSystem&) = delete;

    // Initialize/shutdown
    void init(uint32_t width, uint32_t height, const SSRConfig& config = {});
    void shutdown();
    bool is_initialized() const { return m_initialized; }

    // Resize buffers
    void resize(uint32_t width, uint32_t height);

    // Configuration
    void set_config(const SSRConfig& config) { m_config = config; }
    const SSRConfig& get_config() const { return m_config; }
    SSRConfig& get_config() { return m_config; }

    // Generate hi-z pyramid from depth buffer
    void generate_hiz(bgfx::ViewId view_id, bgfx::TextureHandle depth_texture);

    // Trace reflections
    void trace(bgfx::ViewId view_id,
               bgfx::TextureHandle color_texture,    // Scene color
               bgfx::TextureHandle depth_texture,    // Scene depth
               bgfx::TextureHandle normal_texture,   // G-buffer normals
               bgfx::TextureHandle roughness_texture, // Roughness (or packed PBR)
               const Mat4& view_matrix,
               const Mat4& proj_matrix,
               const Mat4& inv_proj_matrix,
               const Mat4& inv_view_matrix);

    // Temporal resolve (if enabled)
    void temporal_resolve(bgfx::ViewId view_id,
                          bgfx::TextureHandle velocity_texture,  // Motion vectors
                          const Mat4& prev_view_proj);

    // Composite reflections with scene
    void composite(bgfx::ViewId view_id,
                   bgfx::TextureHandle scene_color,
                   bgfx::TextureHandle roughness_texture);

    // Get result texture (for external compositing)
    bgfx::TextureHandle get_reflection_texture() const { return m_reflection_textures[m_history_index]; }
    bgfx::TextureHandle get_hiz_texture() const { return m_hiz_texture; }

    // Full SSR pass (trace + temporal + composite)
    void render(bgfx::ViewId trace_view,
                bgfx::ViewId resolve_view,
                bgfx::ViewId composite_view,
                bgfx::TextureHandle color_texture,
                bgfx::TextureHandle depth_texture,
                bgfx::TextureHandle normal_texture,
                bgfx::TextureHandle roughness_texture,
                bgfx::TextureHandle velocity_texture,
                const Mat4& view_matrix,
                const Mat4& proj_matrix,
                const Mat4& inv_proj_matrix,
                const Mat4& inv_view_matrix,
                const Mat4& prev_view_proj);

    // Statistics
    struct Stats {
        uint32_t trace_width = 0;
        uint32_t trace_height = 0;
        uint32_t hiz_levels = 0;
        float average_ray_steps = 0.0f;
    };
    Stats get_stats() const { return m_stats; }

private:
    void create_textures(uint32_t width, uint32_t height);
    void destroy_textures();
    void create_programs();
    void destroy_programs();

    SSRConfig m_config;
    bool m_initialized = false;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_trace_width = 0;
    uint32_t m_trace_height = 0;

    // Textures — ping-pong pair for temporal resolve
    bgfx::TextureHandle m_reflection_textures[2] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
    int m_history_index = 0;  // m_reflection_textures[m_history_index] = current, [1-m_history_index] = history
    bgfx::TextureHandle m_hiz_texture = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_hit_texture = BGFX_INVALID_HANDLE;  // UV + PDF

    // Framebuffers — pre-created for both ping-pong configurations
    bgfx::FrameBufferHandle m_trace_fbs[2] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
    bgfx::FrameBufferHandle m_resolve_fbs[2] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
    std::vector<bgfx::FrameBufferHandle> m_hiz_fbs;

    // Programs
    bgfx::ProgramHandle m_hiz_program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_trace_program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_resolve_program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_composite_program = BGFX_INVALID_HANDLE;

    // Uniforms
    bgfx::UniformHandle u_ssr_params = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_ssr_params2 = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_view_matrix = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_proj_matrix = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_inv_proj_matrix = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_inv_view_matrix = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_prev_view_proj = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_texel_size = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_hiz_level = BGFX_INVALID_HANDLE;

    bgfx::UniformHandle s_color = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_depth = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_normal = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_roughness = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_hiz = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_reflection = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_history = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_velocity = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_hit = BGFX_INVALID_HANDLE;

    // Frame counter for temporal jitter
    uint32_t m_frame_count = 0;

    // Stats
    Stats m_stats;
};

// Global SSR system instance
SSRSystem& get_ssr_system();

// SSR utility functions
namespace SSRUtils {

// Calculate reflection direction given view direction and normal
inline Vec3 reflect(const Vec3& incident, const Vec3& normal) {
    return incident - 2.0f * dot(incident, normal) * normal;
}

// Calculate fresnel reflectance (Schlick approximation)
inline float fresnel_schlick(float cos_theta, float f0) {
    return f0 + (1.0f - f0) * std::pow(1.0f - cos_theta, 5.0f);
}

// Calculate screen-space ray direction
inline Vec3 get_reflection_ray(const Vec3& view_pos,
                                const Vec3& world_normal,
                                const Mat4& view_matrix) {
    Vec3 view_dir = normalize(view_pos);
    Vec3 view_normal = normalize(Mat3(view_matrix) * world_normal);
    return reflect(view_dir, view_normal);
}

// Determine if SSR should be applied based on roughness
inline bool should_apply_ssr(float roughness, float threshold) {
    return roughness < threshold;
}

// Calculate importance sample direction for rough reflections
inline Vec3 importance_sample_ggx(const Vec2& xi, const Vec3& normal, float roughness) {
    float a = roughness * roughness;

    float phi = 2.0f * 3.14159265f * xi.x;
    float cos_theta = std::sqrt((1.0f - xi.y) / (1.0f + (a * a - 1.0f) * xi.y));
    float sin_theta = std::sqrt(1.0f - cos_theta * cos_theta);

    // Spherical to cartesian
    Vec3 h;
    h.x = cos(phi) * sin_theta;
    h.y = sin(phi) * sin_theta;
    h.z = cos_theta;

    // Tangent space to world space
    Vec3 up = std::abs(normal.z) < 0.999f ? Vec3(0.0f, 0.0f, 1.0f) : Vec3(1.0f, 0.0f, 0.0f);
    Vec3 tangent = normalize(cross(up, normal));
    Vec3 bitangent = cross(normal, tangent);

    return normalize(tangent * h.x + bitangent * h.y + normal * h.z);
}

} // namespace SSRUtils

} // namespace engine::render
