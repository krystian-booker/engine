#pragma once

#include <engine/core/math.hpp>
#include <bgfx/bgfx.h>

namespace engine::render {

using namespace engine::core;

// Motion blur quality presets
enum class MotionBlurQuality : uint8_t {
    Low,      // 4 samples, no tile-based
    Medium,   // 8 samples, tile-based
    High,     // 16 samples, tile-based, neighbor max
    Ultra     // 32 samples, full quality
};

// Motion blur configuration
struct MotionBlurConfig {
    // Intensity and limits
    float intensity = 1.0f;               // Overall blur intensity
    float max_blur_radius = 32.0f;        // Maximum blur radius in pixels
    float min_velocity_threshold = 0.5f;  // Minimum velocity to apply blur

    // Sample count
    uint32_t samples = 16;                // Number of blur samples

    // Motion sources
    bool camera_motion = true;            // Include camera motion
    bool per_object_motion = true;        // Include per-object motion (requires velocity buffer)

    // Tile-based rendering
    bool tile_based = true;               // Use tile-based optimization
    uint32_t tile_size = 20;              // Tile size for neighbor max

    // Depth-aware blur
    bool depth_aware = true;              // Consider depth for blur weight
    float depth_falloff = 1.0f;           // Depth comparison falloff

    // Jitter and filtering
    bool jitter_samples = true;           // Jitter sample positions
    bool soft_z_extent = true;            // Soft Z comparison for better edges

    // Center attenuation (reduce blur in center for games)
    bool center_attenuation = true;       // Reduce blur near screen center
    float center_falloff_start = 0.2f;    // Start reducing at this radius
    float center_falloff_end = 0.5f;      // Full strength at this radius

    // Shutter speed simulation
    float shutter_angle = 180.0f;         // Shutter angle in degrees (180 = 50% exposure)

    // Debug
    bool debug_velocity = false;          // Show velocity visualization

    // Apply preset
    void apply_preset(MotionBlurQuality quality) {
        switch (quality) {
            case MotionBlurQuality::Low:
                samples = 4;
                tile_based = false;
                depth_aware = false;
                max_blur_radius = 16.0f;
                break;
            case MotionBlurQuality::Medium:
                samples = 8;
                tile_based = true;
                depth_aware = true;
                max_blur_radius = 24.0f;
                break;
            case MotionBlurQuality::High:
                samples = 16;
                tile_based = true;
                depth_aware = true;
                max_blur_radius = 32.0f;
                break;
            case MotionBlurQuality::Ultra:
                samples = 32;
                tile_based = true;
                depth_aware = true;
                max_blur_radius = 48.0f;
                jitter_samples = true;
                break;
        }
    }

    // Get shutter fraction (0-1)
    float get_shutter_fraction() const {
        return shutter_angle / 360.0f;
    }
};

// Velocity buffer format
enum class VelocityFormat : uint8_t {
    RG16F,    // 16-bit float (higher precision)
    RG8,      // 8-bit normalized (lower memory)
    RGBA16F   // 16-bit with extra data (depth, confidence)
};

// Motion blur system
class MotionBlurSystem {
public:
    MotionBlurSystem() = default;
    ~MotionBlurSystem();

    // Non-copyable
    MotionBlurSystem(const MotionBlurSystem&) = delete;
    MotionBlurSystem& operator=(const MotionBlurSystem&) = delete;

    // Initialize/shutdown
    void init(uint32_t width, uint32_t height, const MotionBlurConfig& config = {});
    void shutdown();
    bool is_initialized() const { return m_initialized; }

    // Resize buffers
    void resize(uint32_t width, uint32_t height);

    // Configuration
    void set_config(const MotionBlurConfig& config) { m_config = config; }
    const MotionBlurConfig& get_config() const { return m_config; }
    MotionBlurConfig& get_config() { return m_config; }

    // Get velocity buffer for rendering (write per-object velocities here)
    bgfx::TextureHandle get_velocity_buffer() const { return m_velocity_texture; }
    bgfx::FrameBufferHandle get_velocity_framebuffer() const { return m_velocity_fb; }

    // Generate camera velocity from motion (for static objects)
    void generate_camera_velocity(bgfx::ViewId view_id,
                                   bgfx::TextureHandle depth_texture,
                                   const Mat4& current_view_proj,
                                   const Mat4& prev_view_proj,
                                   const Mat4& inv_view_proj);

    // Generate tile max velocity (for neighbor-based blur)
    void generate_tile_max(bgfx::ViewId view_id);

    // Apply motion blur
    void apply(bgfx::ViewId view_id,
               bgfx::TextureHandle color_texture,
               bgfx::TextureHandle depth_texture);

    // Full motion blur pass
    void render(bgfx::ViewId velocity_view,
                bgfx::ViewId tile_view,
                bgfx::ViewId blur_view,
                bgfx::TextureHandle color_texture,
                bgfx::TextureHandle depth_texture,
                const Mat4& current_view_proj,
                const Mat4& prev_view_proj,
                const Mat4& inv_view_proj);

    // Get result (for external compositing)
    bgfx::TextureHandle get_result_texture() const { return m_result_texture; }

    // Statistics
    struct Stats {
        uint32_t tile_count_x = 0;
        uint32_t tile_count_y = 0;
        float max_velocity = 0.0f;
    };
    Stats get_stats() const { return m_stats; }

private:
    void create_textures(uint32_t width, uint32_t height);
    void destroy_textures();
    void create_programs();
    void destroy_programs();

    MotionBlurConfig m_config;
    bool m_initialized = false;

    uint32_t m_width = 0;
    uint32_t m_height = 0;

    // Textures
    bgfx::TextureHandle m_velocity_texture = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_tile_max_texture = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_neighbor_max_texture = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_result_texture = BGFX_INVALID_HANDLE;

    // Framebuffers
    bgfx::FrameBufferHandle m_velocity_fb = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle m_tile_max_fb = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle m_neighbor_max_fb = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle m_result_fb = BGFX_INVALID_HANDLE;

    // Programs
    bgfx::ProgramHandle m_camera_velocity_program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_tile_max_program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_neighbor_max_program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_blur_program = BGFX_INVALID_HANDLE;

    // Uniforms
    bgfx::UniformHandle u_motion_params = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_motion_params2 = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_view_proj = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_prev_view_proj = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_inv_view_proj = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_texel_size = BGFX_INVALID_HANDLE;

    bgfx::UniformHandle s_color = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_depth = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_velocity = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_tile_max = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_neighbor_max = BGFX_INVALID_HANDLE;

    // Frame counter for jitter
    uint32_t m_frame_count = 0;

    // Stats
    Stats m_stats;
};

// Global motion blur system
MotionBlurSystem& get_motion_blur_system();

// Motion blur utilities
namespace MotionBlurUtils {

// Calculate velocity from current and previous world positions
inline Vec2 calculate_velocity(const Vec3& world_pos,
                                const Mat4& current_view_proj,
                                const Mat4& prev_view_proj) {
    // Current screen position
    Vec4 curr_clip = current_view_proj * Vec4(world_pos, 1.0f);
    Vec2 curr_ndc = Vec2(curr_clip.x / curr_clip.w, curr_clip.y / curr_clip.w);

    // Previous screen position
    Vec4 prev_clip = prev_view_proj * Vec4(world_pos, 1.0f);
    Vec2 prev_ndc = Vec2(prev_clip.x / prev_clip.w, prev_clip.y / prev_clip.w);

    // Velocity in NDC space
    return (curr_ndc - prev_ndc) * 0.5f;  // Convert to UV space
}

// Calculate velocity from current and previous world positions with movement
inline Vec2 calculate_object_velocity(const Vec3& curr_world_pos,
                                       const Vec3& prev_world_pos,
                                       const Mat4& current_view_proj,
                                       const Mat4& prev_view_proj) {
    // Current screen position
    Vec4 curr_clip = current_view_proj * Vec4(curr_world_pos, 1.0f);
    Vec2 curr_ndc = Vec2(curr_clip.x / curr_clip.w, curr_clip.y / curr_clip.w);

    // Previous screen position of previous world position
    Vec4 prev_clip = prev_view_proj * Vec4(prev_world_pos, 1.0f);
    Vec2 prev_ndc = Vec2(prev_clip.x / prev_clip.w, prev_clip.y / prev_clip.w);

    return (curr_ndc - prev_ndc) * 0.5f;
}

// Encode velocity to RG16F format
inline Vec2 encode_velocity(const Vec2& velocity, float max_velocity = 32.0f) {
    return Vec2(
        velocity.x / max_velocity * 0.5f + 0.5f,
        velocity.y / max_velocity * 0.5f + 0.5f
    );
}

// Decode velocity from RG16F format
inline Vec2 decode_velocity(const Vec2& encoded, float max_velocity = 32.0f) {
    return Vec2(
        (encoded.x * 2.0f - 1.0f) * max_velocity,
        (encoded.y * 2.0f - 1.0f) * max_velocity
    );
}

// Calculate blur weight based on velocity length
inline float calculate_blur_weight(const Vec2& velocity, float min_threshold, float max_radius) {
    float vel_length = length(velocity);
    if (vel_length < min_threshold) return 0.0f;
    return std::min(vel_length / max_radius, 1.0f);
}

// Calculate center attenuation
inline float calculate_center_attenuation(const Vec2& uv,
                                           float start_radius,
                                           float end_radius) {
    Vec2 centered = uv - Vec2(0.5f);
    float dist = length(centered) * 2.0f;  // 0-1 range from center to edge

    if (dist < start_radius) return 0.0f;
    if (dist > end_radius) return 1.0f;

    return (dist - start_radius) / (end_radius - start_radius);
}

// Depth comparison for scatter-as-gather
inline float soft_depth_compare(float depth_a, float depth_b, float falloff) {
    return std::exp(-std::abs(depth_a - depth_b) * falloff);
}

} // namespace MotionBlurUtils

// Component for per-object motion vectors
struct MotionVectorComponent {
    Mat4 prev_transform = Mat4(1.0f);  // Previous frame's world transform
    bool first_frame = true;           // Skip first frame (no previous data)
    bool enabled = true;               // Enable motion vectors for this object
};

} // namespace engine::render
