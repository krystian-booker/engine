#include <engine/render/shadow_system.hpp>
#include <engine/render/renderer.hpp>
#include <engine/core/log.hpp>
#include <algorithm>
#include <cmath>

namespace engine::render {

using namespace engine::core;

ShadowSystem::~ShadowSystem() {
    if (m_initialized) {
        shutdown();
    }
}

void ShadowSystem::init(IRenderer* renderer, const ShadowConfig& config) {
    m_renderer = renderer;
    m_config = config;
    m_initialized = true;

    create_cascade_render_targets();

    log(LogLevel::Info, "Shadow system initialized");
}

void ShadowSystem::shutdown() {
    if (!m_initialized) return;

    destroy_cascade_render_targets();

    if (m_shadow_atlas.valid()) {
        m_renderer->destroy_render_target(m_shadow_atlas);
    }

    m_initialized = false;
    m_renderer = nullptr;

    log(LogLevel::Info, "Shadow system shutdown");
}

void ShadowSystem::set_config(const ShadowConfig& config) {
    bool needs_recreate = (config.cascade_resolution != m_config.cascade_resolution ||
                           config.cascade_count != m_config.cascade_count);

    m_config = config;

    if (needs_recreate && m_initialized) {
        destroy_cascade_render_targets();
        create_cascade_render_targets();
    }
}

void ShadowSystem::create_cascade_render_targets() {
    for (uint32_t i = 0; i < m_config.cascade_count && i < 4; ++i) {
        RenderTargetDesc desc;
        desc.width = m_config.cascade_resolution;
        desc.height = m_config.cascade_resolution;
        desc.color_attachment_count = 0;  // Depth only
        desc.has_depth = true;
        desc.depth_format = TextureFormat::Depth32F;
        desc.samplable = true;
        desc.debug_name = "ShadowCascade";

        m_cascade_render_targets[i] = m_renderer->create_render_target(desc);

        // Configure the view for this cascade
        ViewConfig view_config;
        view_config.render_target = m_cascade_render_targets[i];
        view_config.clear_color_enabled = false;
        view_config.clear_depth_enabled = true;
        view_config.clear_depth = 1.0f;

        m_renderer->configure_view(get_cascade_view(i), view_config);
    }
}

void ShadowSystem::destroy_cascade_render_targets() {
    for (uint32_t i = 0; i < 4; ++i) {
        if (m_cascade_render_targets[i].valid()) {
            m_renderer->destroy_render_target(m_cascade_render_targets[i]);
            m_cascade_render_targets[i] = RenderTargetHandle{};
        }
    }
}

void ShadowSystem::update_cascades(const Mat4& camera_view, const Mat4& camera_proj,
                                    const Vec3& light_direction, float camera_near, float camera_far) {
    // Calculate cascade split distances
    calculate_cascade_split_distances(camera_near, camera_far);

    Vec3 light_dir = glm::normalize(light_direction);

    for (uint32_t i = 0; i < m_config.cascade_count && i < 4; ++i) {
        float near_split = m_cascade_distances[i];
        float far_split = m_cascade_distances[i + 1];

        // Get frustum corners for this cascade
        auto corners = shadow::get_frustum_corners_world_space(camera_view, camera_proj,
                                                                near_split, far_split);

        // Calculate frustum center
        Vec3 center(0.0f);
        for (const auto& corner : corners) {
            center += corner;
        }
        center /= 8.0f;

        // Create light view matrix
        Mat4 light_view = glm::lookAt(center - light_dir * 100.0f, center, Vec3(0.0f, 1.0f, 0.0f));

        // Calculate bounds in light space
        Vec3 min_bounds(std::numeric_limits<float>::max());
        Vec3 max_bounds(std::numeric_limits<float>::lowest());

        for (const auto& corner : corners) {
            Vec4 light_space = light_view * Vec4(corner, 1.0f);
            min_bounds = glm::min(min_bounds, Vec3(light_space));
            max_bounds = glm::max(max_bounds, Vec3(light_space));
        }

        // Create stable orthographic projection
        Mat4 light_proj = shadow::create_stable_ortho_projection(min_bounds, max_bounds,
                                                                  m_config.cascade_resolution);

        // Store cascade data
        m_cascades[i].view_proj = light_proj * light_view;
        m_cascades[i].split_distance = far_split;

        // Calculate bounding sphere for culling
        float radius = glm::length(max_bounds - min_bounds) * 0.5f;
        m_cascades[i].sphere = Vec4(center, radius);

        // Update view transform for rendering
        m_renderer->set_view_transform(get_cascade_view(i), light_view, light_proj);
    }
}

void ShadowSystem::calculate_cascade_split_distances(float near, float far) {
    m_cascade_distances[0] = near;

    for (uint32_t i = 0; i < m_config.cascade_count && i < 4; ++i) {
        float p = static_cast<float>(i + 1) / static_cast<float>(m_config.cascade_count);

        // Logarithmic-linear split scheme
        float log_split = near * std::pow(far / near, p);
        float linear_split = near + (far - near) * p;

        // Blend between logarithmic and linear (0.5 = 50% each)
        float lambda = 0.5f;
        float split = lambda * log_split + (1.0f - lambda) * linear_split;

        // Apply user-defined split override if specified
        if (m_config.cascade_splits[i] > 0.0f) {
            split = near + (far - near) * m_config.cascade_splits[i];
        }

        m_cascade_distances[i + 1] = split;
    }
}

RenderTargetHandle ShadowSystem::get_cascade_render_target(uint32_t index) const {
    if (index < 4) {
        return m_cascade_render_targets[index];
    }
    return RenderTargetHandle{};
}

std::array<Mat4, 4> ShadowSystem::get_cascade_matrices() const {
    std::array<Mat4, 4> matrices;
    for (uint32_t i = 0; i < 4; ++i) {
        matrices[i] = m_cascades[i].view_proj;
    }
    return matrices;
}

Vec4 ShadowSystem::get_cascade_splits() const {
    return Vec4(m_cascade_distances[1], m_cascade_distances[2],
                m_cascade_distances[3], m_cascade_distances[4]);
}

RenderView ShadowSystem::get_cascade_view(uint32_t cascade) const {
    switch (cascade) {
        case 0: return RenderView::ShadowCascade0;
        case 1: return RenderView::ShadowCascade1;
        case 2: return RenderView::ShadowCascade2;
        case 3: return RenderView::ShadowCascade3;
        default: return RenderView::ShadowCascade0;
    }
}

RenderTargetHandle ShadowSystem::allocate_shadow_map(uint32_t light_index, uint8_t /*light_type*/) {
    // For now, return cascade 0 - TODO: implement proper atlas allocation
    if (light_index < 4) {
        return m_cascade_render_targets[light_index];
    }
    return RenderTargetHandle{};
}

void ShadowSystem::free_shadow_map(uint32_t /*light_index*/) {
    // TODO: implement proper atlas deallocation
}

void ShadowSystem::resize(uint32_t new_resolution) {
    m_config.cascade_resolution = new_resolution;

    if (m_initialized) {
        destroy_cascade_render_targets();
        create_cascade_render_targets();
    }
}

// Helper namespace implementations
namespace shadow {

std::array<Vec3, 8> get_frustum_corners_world_space(const Mat4& view, const Mat4& proj) {
    Mat4 inv = glm::inverse(proj * view);

    std::array<Vec3, 8> corners;
    int index = 0;

    for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < 2; ++y) {
            for (int z = 0; z < 2; ++z) {
                Vec4 pt = inv * Vec4(
                    2.0f * x - 1.0f,
                    2.0f * y - 1.0f,
                    2.0f * z - 1.0f,
                    1.0f
                );
                corners[index++] = Vec3(pt) / pt.w;
            }
        }
    }

    return corners;
}

std::array<Vec3, 8> get_frustum_corners_world_space(const Mat4& view, const Mat4& proj,
                                                     float near_plane, float far_plane) {
    // Get the original projection matrix parameters
    float fov_y = 2.0f * std::atan(1.0f / proj[1][1]);
    float aspect = proj[1][1] / proj[0][0];

    // Create modified projection for this depth range
    Mat4 sub_proj = glm::perspective(fov_y, aspect, near_plane, far_plane);

    return get_frustum_corners_world_space(view, sub_proj);
}

void calculate_light_ortho_bounds(const std::vector<Vec3>& corners, const Mat4& light_view,
                                  Vec3& min_bounds, Vec3& max_bounds) {
    min_bounds = Vec3(std::numeric_limits<float>::max());
    max_bounds = Vec3(std::numeric_limits<float>::lowest());

    for (const auto& corner : corners) {
        Vec4 light_space = light_view * Vec4(corner, 1.0f);
        min_bounds = glm::min(min_bounds, Vec3(light_space));
        max_bounds = glm::max(max_bounds, Vec3(light_space));
    }
}

Mat4 create_stable_ortho_projection(const Vec3& min_bounds, const Vec3& max_bounds,
                                    uint32_t shadow_map_size) {
    // Calculate world units per texel
    float world_units_per_texel = (max_bounds.x - min_bounds.x) / static_cast<float>(shadow_map_size);

    // Snap to texel grid to reduce shadow edge shimmering
    Vec3 snapped_min, snapped_max;
    snapped_min.x = std::floor(min_bounds.x / world_units_per_texel) * world_units_per_texel;
    snapped_min.y = std::floor(min_bounds.y / world_units_per_texel) * world_units_per_texel;
    snapped_min.z = min_bounds.z;

    snapped_max.x = std::floor(max_bounds.x / world_units_per_texel) * world_units_per_texel;
    snapped_max.y = std::floor(max_bounds.y / world_units_per_texel) * world_units_per_texel;
    snapped_max.z = max_bounds.z;

    // Extend depth range a bit to avoid near-plane clipping
    float z_mult = 10.0f;
    float z_range = snapped_max.z - snapped_min.z;

    return glm::ortho(
        snapped_min.x, snapped_max.x,
        snapped_min.y, snapped_max.y,
        snapped_min.z - z_range * z_mult,
        snapped_max.z + z_range * z_mult
    );
}

} // namespace shadow

} // namespace engine::render
