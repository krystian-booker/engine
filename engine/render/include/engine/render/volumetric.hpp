#pragma once

#include <engine/render/types.hpp>
#include <engine/render/render_target.hpp>
#include <engine/core/math.hpp>
#include <array>

namespace engine::render {

using namespace engine::core;

class IRenderer;

// Volumetric lighting/fog configuration
struct VolumetricConfig {
    // Quality settings
    uint32_t froxel_width = 160;      // Volume texture width
    uint32_t froxel_height = 90;      // Volume texture height
    uint32_t froxel_depth = 128;      // Volume texture depth slices

    // Fog settings
    float fog_density = 0.01f;        // Base fog density
    Vec3 fog_albedo{1.0f};            // Fog color/albedo
    float fog_height_falloff = 0.1f;  // Density falloff with height
    float fog_base_height = 0.0f;     // Height at which fog density is maximum

    // Scattering settings
    float scattering_intensity = 1.0f;
    float anisotropy = 0.5f;          // Henyey-Greenstein phase function g (-1 to 1)
    float extinction_coefficient = 0.01f;

    // Light settings
    float light_intensity_scale = 1.0f;
    bool shadows_enabled = true;
    int shadow_samples = 4;           // Shadow ray samples

    // Temporal settings
    bool temporal_reprojection = true;
    float temporal_blend = 0.9f;      // History blend factor

    // Distance settings
    float near_plane = 0.1f;
    float far_plane = 100.0f;

    // Noise settings
    float noise_scale = 0.1f;
    float noise_intensity = 0.2f;
    bool animated_noise = true;
};

// Volumetric light data (for injection into volume)
struct VolumetricLightData {
    Vec3 position;
    Vec3 direction;
    Vec3 color;
    float intensity;
    float range;
    float spot_angle_cos;
    uint8_t type;  // 0=directional, 1=point, 2=spot
    int shadow_cascade;  // -1 if no shadow, 0-3 for cascade index
};

// Volumetric fog system
// Implements froxel-based volumetric lighting with temporal reprojection
class VolumetricSystem {
public:
    VolumetricSystem() = default;
    ~VolumetricSystem();

    // Initialize volumetric system
    void init(IRenderer* renderer, const VolumetricConfig& config);
    void shutdown();

    // Configuration
    void set_config(const VolumetricConfig& config);
    const VolumetricConfig& get_config() const { return m_config; }

    // Update volumetrics (call each frame)
    void update(
        const Mat4& view_matrix,
        const Mat4& proj_matrix,
        const Mat4& prev_view_proj,  // For temporal reprojection
        TextureHandle depth_texture,
        const std::array<TextureHandle, 4>& shadow_maps,
        const std::array<Mat4, 4>& shadow_matrices
    );

    // Set lights for volumetric rendering
    void set_lights(const std::vector<VolumetricLightData>& lights);

    // Get the integrated volumetric texture (for applying in main pass)
    TextureHandle get_volumetric_texture() const;

    // Get froxel volume texture (for debug visualization)
    TextureHandle get_froxel_texture() const;

    // Resize volumetric render targets
    void resize(uint32_t width, uint32_t height);

    // Get render views
    RenderView get_scatter_view() const { return RenderView::VolumetricScatter; }
    RenderView get_integration_view() const { return RenderView::VolumetricIntegrate; }

private:
    void create_render_targets();
    void destroy_render_targets();
    void create_noise_texture();

    // Render passes
    void inject_density_pass();
    void scatter_light_pass();
    void temporal_filter_pass();
    void spatial_filter_pass();
    void integration_pass();

    IRenderer* m_renderer = nullptr;
    VolumetricConfig m_config;
    bool m_initialized = false;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_frame_count = 0;

    // Froxel volume textures (3D textures)
    RenderTargetHandle m_density_volume;       // Density injection
    RenderTargetHandle m_scatter_volume;       // In-scattered light
    RenderTargetHandle m_integrated_volume;    // Front-to-back integration

    // Temporal history
    RenderTargetHandle m_history_volume[2];
    int m_history_index = 0;

    // Final 2D result
    RenderTargetHandle m_volumetric_result;

    // Noise texture for temporal jittering
    TextureHandle m_noise_texture;
    TextureHandle m_blue_noise;

    // Light data for current frame
    std::vector<VolumetricLightData> m_lights;

    // Previous frame data for reprojection
    Mat4 m_prev_view_proj{1.0f};

    // Current frame data (stored in update() for use in passes)
    TextureHandle m_depth_texture;
    std::array<TextureHandle, 4> m_shadow_maps;
    std::array<Mat4, 4> m_shadow_matrices;
    Vec3 m_camera_pos{0.0f};
    float m_near_plane = 0.1f;
    float m_far_plane = 100.0f;
};

// Henyey-Greenstein phase function helpers
namespace phase {

// Henyey-Greenstein phase function
// g: anisotropy parameter (-1 = back-scatter, 0 = isotropic, 1 = forward-scatter)
// cos_theta: cosine of angle between light and view direction
float henyey_greenstein(float cos_theta, float g);

// Schlick approximation of Henyey-Greenstein (faster)
float schlick_phase(float cos_theta, float g);

// Cornette-Shanks phase function (more physically accurate for water droplets)
float cornette_shanks(float cos_theta, float g);

} // namespace phase

// Volumetric noise generation
namespace volumetric_noise {

// Generate 3D Perlin noise for density variation
void generate_3d_noise(std::vector<uint8_t>& data, uint32_t size);

// Generate blue noise texture for temporal jittering
void generate_blue_noise(std::vector<uint8_t>& data, uint32_t size);

} // namespace volumetric_noise

} // namespace engine::render
