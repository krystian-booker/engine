#pragma once

#include <engine/core/math.hpp>
#include <bgfx/bgfx.h>
#include <algorithm>

namespace engine::render {

using namespace engine::core;

// DOF quality presets
enum class DOFQuality : uint8_t {
    Low,      // Gaussian blur only, no bokeh
    Medium,   // Gaussian with soft bokeh
    High,     // Full bokeh simulation
    Ultra     // High sample count bokeh
};

// DOF mode
enum class DOFMode : uint8_t {
    Gaussian,       // Fast Gaussian blur (best for performance)
    Bokeh,          // Physical bokeh simulation
    BokehSprites    // Sprite-based bokeh (most realistic)
};

// Bokeh shape
enum class BokehShape : uint8_t {
    Circle,
    Hexagon,
    Octagon,
    Custom   // Uses custom texture
};

// DOF configuration
struct DOFConfig {
    // Focus settings
    float focus_distance = 10.0f;         // Distance to focus plane (world units)
    float focus_range = 5.0f;             // Range around focus that stays sharp
    float near_blur_start = 0.0f;         // Near blur starts at this distance
    float far_blur_start = 20.0f;         // Far blur starts at this distance

    // Auto-focus
    bool auto_focus = false;              // Enable auto-focus
    Vec2 auto_focus_point = Vec2(0.5f);   // Screen-space focus point (0-1)
    float auto_focus_speed = 5.0f;        // Focus transition speed
    float auto_focus_range = 1000.0f;     // Maximum auto-focus distance

    // Blur settings
    float max_blur_radius = 8.0f;         // Maximum blur radius in pixels
    float near_blur_intensity = 1.0f;     // Near blur intensity
    float far_blur_intensity = 1.0f;      // Far blur intensity

    // Bokeh settings
    DOFMode mode = DOFMode::Gaussian;
    BokehShape bokeh_shape = BokehShape::Circle;
    float bokeh_brightness = 1.0f;        // Bokeh highlight brightness
    float bokeh_threshold = 0.5f;         // Brightness threshold for bokeh
    float bokeh_size = 1.0f;              // Bokeh size multiplier
    uint32_t bokeh_samples = 32;          // Samples for bokeh blur
    float bokeh_rotation = 0.0f;          // Bokeh shape rotation (degrees)

    // Aperture simulation
    float aperture = 2.8f;                // f-number (lower = more blur)
    float focal_length = 50.0f;           // Focal length in mm
    float sensor_height = 24.0f;          // Sensor height in mm (for FOV calculation)

    // Quality settings
    bool high_quality_near = true;        // High quality near blur (prevents halos)
    bool chromatic_aberration = false;    // Simulate CA on bokeh edges
    float ca_intensity = 0.01f;           // Chromatic aberration intensity

    // Debug
    bool debug_coc = false;               // Visualize circle of confusion
    bool debug_focus = false;             // Show focus plane

    // Apply preset
    void apply_preset(DOFQuality quality) {
        switch (quality) {
            case DOFQuality::Low:
                mode = DOFMode::Gaussian;
                max_blur_radius = 4.0f;
                high_quality_near = false;
                bokeh_samples = 8;
                break;
            case DOFQuality::Medium:
                mode = DOFMode::Gaussian;
                max_blur_radius = 6.0f;
                high_quality_near = true;
                bokeh_samples = 16;
                break;
            case DOFQuality::High:
                mode = DOFMode::Bokeh;
                max_blur_radius = 8.0f;
                high_quality_near = true;
                bokeh_samples = 32;
                break;
            case DOFQuality::Ultra:
                mode = DOFMode::Bokeh;
                max_blur_radius = 12.0f;
                high_quality_near = true;
                bokeh_samples = 64;
                chromatic_aberration = true;
                break;
        }
    }

    // Calculate CoC from physical camera params
    float calculate_max_coc() const {
        // Based on thin lens equation
        float coc = (focal_length * focal_length) /
                    (aperture * (focus_distance * 1000.0f - focal_length));
        return coc * (sensor_height / 24.0f);  // Normalized to sensor
    }
};

// DOF system
class DOFSystem {
public:
    DOFSystem() = default;
    ~DOFSystem();

    // Non-copyable
    DOFSystem(const DOFSystem&) = delete;
    DOFSystem& operator=(const DOFSystem&) = delete;

    // Initialize/shutdown
    void init(uint32_t width, uint32_t height, const DOFConfig& config = {});
    void shutdown();
    bool is_initialized() const { return m_initialized; }

    // Resize buffers
    void resize(uint32_t width, uint32_t height);

    // Configuration
    void set_config(const DOFConfig& config) { m_config = config; }
    const DOFConfig& get_config() const { return m_config; }
    DOFConfig& get_config() { return m_config; }

    // Update (for auto-focus)
    void update(float dt, bgfx::TextureHandle depth_texture,
                const Mat4& inv_proj_matrix);

    // Calculate Circle of Confusion
    void calculate_coc(bgfx::ViewId view_id,
                       bgfx::TextureHandle depth_texture,
                       const Mat4& proj_matrix);

    // Downsample and separate near/far
    void downsample(bgfx::ViewId view_id,
                    bgfx::TextureHandle color_texture);

    // Blur near field
    void blur_near(bgfx::ViewId view_id);

    // Blur far field
    void blur_far(bgfx::ViewId view_id);

    // Apply bokeh effect (if using bokeh mode)
    void apply_bokeh(bgfx::ViewId view_id,
                     bgfx::TextureHandle color_texture);

    // Composite final result
    void composite(bgfx::ViewId view_id,
                   bgfx::TextureHandle color_texture);

    // Full DOF pass
    void render(bgfx::ViewId coc_view,
                bgfx::ViewId downsample_view,
                bgfx::ViewId blur_view,
                bgfx::ViewId composite_view,
                bgfx::TextureHandle color_texture,
                bgfx::TextureHandle depth_texture,
                const Mat4& proj_matrix);

    // Get result
    bgfx::TextureHandle get_result_texture() const { return m_result_texture; }
    bgfx::TextureHandle get_coc_texture() const { return m_coc_texture; }

    // Focus control
    void set_focus_distance(float distance);
    void focus_on_world_point(const Vec3& world_pos, const Mat4& view_matrix);
    float get_current_focus_distance() const { return m_current_focus_distance; }

    // Statistics
    struct Stats {
        float current_focus = 0.0f;
        float target_focus = 0.0f;
        float max_near_coc = 0.0f;
        float max_far_coc = 0.0f;
    };
    Stats get_stats() const { return m_stats; }

private:
    void create_textures(uint32_t width, uint32_t height);
    void destroy_textures();
    void create_programs();
    void destroy_programs();
    float sample_depth_at_focus_point(bgfx::TextureHandle depth_texture,
                                       const Mat4& inv_proj_matrix);

    DOFConfig m_config;
    bool m_initialized = false;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_half_width = 0;
    uint32_t m_half_height = 0;

    // Current focus state
    float m_current_focus_distance = 10.0f;
    float m_target_focus_distance = 10.0f;

    // Textures
    bgfx::TextureHandle m_coc_texture = BGFX_INVALID_HANDLE;         // Circle of confusion
    bgfx::TextureHandle m_near_texture = BGFX_INVALID_HANDLE;        // Near field (half res)
    bgfx::TextureHandle m_far_texture = BGFX_INVALID_HANDLE;         // Far field (half res)
    bgfx::TextureHandle m_near_blur_texture = BGFX_INVALID_HANDLE;   // Blurred near
    bgfx::TextureHandle m_far_blur_texture = BGFX_INVALID_HANDLE;    // Blurred far
    bgfx::TextureHandle m_result_texture = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_bokeh_texture = BGFX_INVALID_HANDLE;       // Custom bokeh shape

    // Framebuffers
    bgfx::FrameBufferHandle m_coc_fb = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle m_downsample_fb = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle m_near_blur_fb = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle m_far_blur_fb = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle m_result_fb = BGFX_INVALID_HANDLE;

    // Programs
    bgfx::ProgramHandle m_coc_program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_downsample_program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_blur_program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_bokeh_program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_composite_program = BGFX_INVALID_HANDLE;

    // Uniforms
    bgfx::UniformHandle u_dof_params = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_dof_params2 = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_dof_focus = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_texel_size = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_proj_params = BGFX_INVALID_HANDLE;

    bgfx::UniformHandle s_color = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_depth = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_coc = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_near = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_far = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_near_blur = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_far_blur = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_bokeh = BGFX_INVALID_HANDLE;

    // Stats
    Stats m_stats;
};

// Global DOF system
DOFSystem& get_dof_system();

// DOF utilities
namespace DOFUtils {

// Calculate circle of confusion from depth
inline float calculate_coc(float depth,
                            float focus_distance,
                            float aperture,
                            float focal_length,
                            float max_coc) {
    if (depth <= 0.0f) return 0.0f;

    // Thin lens equation
    float coc = std::abs(aperture * (focal_length * (focus_distance - depth)) /
                         (depth * (focus_distance - focal_length)));

    // Normalize to max CoC
    return std::min(coc / max_coc, 1.0f);
}

// Calculate signed CoC (negative = near, positive = far)
inline float calculate_signed_coc(float depth,
                                   float focus_distance,
                                   float focus_range,
                                   float max_coc) {
    if (depth <= 0.0f) return 0.0f;

    float signed_distance = depth - focus_distance;

    // In-focus range
    if (std::abs(signed_distance) < focus_range * 0.5f) {
        return 0.0f;
    }

    // Calculate CoC with sign
    float coc = (signed_distance - focus_range * 0.5f * (signed_distance > 0 ? 1.0f : -1.0f)) /
                (max_coc * (signed_distance > 0 ? 1.0f : -1.0f));

    return std::clamp(coc, -1.0f, 1.0f);
}

// Linear depth from depth buffer value
inline float linear_depth(float depth_buffer_value,
                           float near_plane,
                           float far_plane) {
    return near_plane * far_plane /
           (far_plane - depth_buffer_value * (far_plane - near_plane));
}

// Generate bokeh kernel for disk sampling
inline void generate_disk_kernel(Vec2* samples, uint32_t count) {
    float golden_angle = 2.399963f;  // Golden angle in radians

    for (uint32_t i = 0; i < count; ++i) {
        float r = std::sqrt(static_cast<float>(i + 0.5f) / count);
        float theta = golden_angle * i;

        samples[i].x = r * std::cos(theta);
        samples[i].y = r * std::sin(theta);
    }
}

// Generate hexagonal bokeh kernel
inline void generate_hex_kernel(Vec2* samples, uint32_t count) {
    // Hexagonal pattern using rings
    uint32_t sample_idx = 0;
    samples[sample_idx++] = Vec2(0.0f, 0.0f);  // Center

    int ring = 1;
    while (sample_idx < count) {
        float r = static_cast<float>(ring) / std::sqrt(static_cast<float>(count));

        for (int side = 0; side < 6 && sample_idx < count; ++side) {
            float angle_start = side * 3.14159f / 3.0f;
            float angle_end = (side + 1) * 3.14159f / 3.0f;

            int points_per_side = ring;
            for (int p = 0; p < points_per_side && sample_idx < count; ++p) {
                float t = static_cast<float>(p) / points_per_side;
                float angle = angle_start + t * (angle_end - angle_start);

                samples[sample_idx].x = r * std::cos(angle);
                samples[sample_idx].y = r * std::sin(angle);
                sample_idx++;
            }
        }
        ring++;
    }
}

// Calculate focus distance from world position
inline float focus_distance_from_world(const Vec3& world_pos,
                                        const Mat4& view_matrix) {
    // Transform to view space
    Vec4 view_pos = view_matrix * Vec4(world_pos, 1.0f);
    return -view_pos.z;  // Negative Z is forward in view space
}

// Smoothstep for CoC transitions
inline float smoothstep_coc(float edge0, float edge1, float x) {
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

} // namespace DOFUtils

// Component for per-camera DOF settings
struct DOFComponent {
    DOFConfig config;
    bool enabled = true;
    bool override_global = false;
};

} // namespace engine::render
