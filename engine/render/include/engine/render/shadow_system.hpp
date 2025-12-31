#pragma once

#include <engine/render/types.hpp>
#include <engine/render/render_target.hpp>
#include <engine/core/math.hpp>
#include <array>
#include <vector>

namespace engine::render {

using namespace engine::core;

class IRenderer;

// Shadow map configuration
struct ShadowConfig {
    // Cascaded Shadow Map settings (for directional lights)
    uint32_t cascade_count = 4;
    uint32_t cascade_resolution = 2048;
    std::array<float, 4> cascade_splits = {0.05f, 0.15f, 0.35f, 1.0f};

    // Point/spot light shadow settings
    uint32_t point_light_resolution = 512;
    uint32_t spot_light_resolution = 1024;
    uint32_t max_shadow_casting_lights = 4;

    // Quality settings
    float shadow_bias = 0.001f;
    float normal_bias = 0.01f;
    float cascade_blend_distance = 0.1f;  // Blend between cascades
    bool pcf_enabled = true;
    uint32_t pcf_samples = 16;  // For PCF filtering
};

// Per-cascade shadow data
struct CascadeData {
    Mat4 view_proj;         // Light view-projection matrix
    float split_distance;   // Far plane distance for this cascade
    Vec4 sphere;            // Bounding sphere (xyz = center, w = radius)
};

// Shadow map data for a single light
struct ShadowMapData {
    RenderTargetHandle render_target;
    TextureHandle depth_texture;
    Mat4 light_matrix;       // For spot/directional: single matrix
    std::array<Mat4, 6> cube_matrices;  // For point lights: 6 face matrices
};

// Shadow system manages shadow map rendering
class ShadowSystem {
public:
    ShadowSystem() = default;
    ~ShadowSystem();

    // Initialize with configuration
    void init(IRenderer* renderer, const ShadowConfig& config);
    void shutdown();

    // Get configuration
    const ShadowConfig& get_config() const { return m_config; }
    void set_config(const ShadowConfig& config);

    // Cascade shadow map management
    void update_cascades(const Mat4& camera_view, const Mat4& camera_proj,
                         const Vec3& light_direction, float camera_near, float camera_far);
    const CascadeData& get_cascade(uint32_t index) const { return m_cascades[index]; }
    RenderTargetHandle get_cascade_render_target(uint32_t index) const;
    TextureHandle get_shadow_atlas() const { return m_shadow_atlas_texture; }

    // Get cascade matrices for shader upload
    std::array<Mat4, 4> get_cascade_matrices() const;
    Vec4 get_cascade_splits() const;

    // Shadow atlas management for point/spot lights
    RenderTargetHandle allocate_shadow_map(uint32_t light_index, uint8_t light_type);
    void free_shadow_map(uint32_t light_index);

    // Get the render view for a cascade
    RenderView get_cascade_view(uint32_t cascade) const;

    // Resize shadow maps (call on resolution change)
    void resize(uint32_t new_resolution);

private:
    void create_cascade_render_targets();
    void destroy_cascade_render_targets();
    void calculate_cascade_split_distances(float near, float far);
    Mat4 calculate_light_matrix(const Vec3& light_dir, const std::vector<Vec3>& frustum_corners);

    IRenderer* m_renderer = nullptr;
    ShadowConfig m_config;
    bool m_initialized = false;

    // Cascade shadow maps
    std::array<RenderTargetHandle, 4> m_cascade_render_targets;
    std::array<CascadeData, 4> m_cascades;
    std::array<float, 5> m_cascade_distances;  // Near, split1, split2, split3, far

    // Shadow atlas for point/spot lights
    RenderTargetHandle m_shadow_atlas;
    TextureHandle m_shadow_atlas_texture;
    std::vector<bool> m_atlas_slots;  // Which slots are in use
};

// Helper functions for shadow calculations
namespace shadow {

// Calculate frustum corners in world space
std::array<Vec3, 8> get_frustum_corners_world_space(const Mat4& view, const Mat4& proj);

// Calculate frustum corners for a specific depth range
std::array<Vec3, 8> get_frustum_corners_world_space(const Mat4& view, const Mat4& proj,
                                                     float near_plane, float far_plane);

// Calculate tight bounding box for frustum corners
void calculate_light_ortho_bounds(const std::vector<Vec3>& corners, const Mat4& light_view,
                                  Vec3& min_bounds, Vec3& max_bounds);

// Stable cascade shadow map projection (reduces shimmering)
Mat4 create_stable_ortho_projection(const Vec3& min_bounds, const Vec3& max_bounds,
                                    uint32_t shadow_map_size);

} // namespace shadow

} // namespace engine::render
