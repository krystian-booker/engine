#include <engine/ui/ui_world_canvas.hpp>
#include <engine/render/render_pipeline.hpp>
#include <cmath>
#include <algorithm>

namespace engine::ui {

UIWorldCanvas::UIWorldCanvas() {
    // World canvases default to smaller size for HUD elements
    set_size(200, 50);
}

void UIWorldCanvas::set_world_position(const Vec3& position) {
    m_world_position = position;
}

void UIWorldCanvas::set_world_rotation(const Quat& rotation) {
    m_world_rotation = rotation;
}

void UIWorldCanvas::set_world_scale(float scale) {
    m_world_scale = scale;
}

Vec2 UIWorldCanvas::project_to_screen(const Vec3& world_pos,
                                        const Mat4& view_projection,
                                        uint32_t screen_width,
                                        uint32_t screen_height,
                                        bool* out_behind_camera) {
    // Transform to clip space
    Vec4 clip = view_projection * Vec4(world_pos, 1.0f);

    // Check if behind camera
    bool behind = clip.w <= 0.0f;
    if (out_behind_camera) {
        *out_behind_camera = behind;
    }

    if (behind) {
        return Vec2(-10000.0f, -10000.0f); // Off-screen
    }

    // Perspective divide to get NDC
    Vec3 ndc = Vec3(clip.x, clip.y, clip.z) / clip.w;

    // NDC to screen coordinates
    // NDC is in range [-1, 1], Y is up in NDC but down in screen coords
    float screen_x = (ndc.x * 0.5f + 0.5f) * static_cast<float>(screen_width);
    float screen_y = (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(screen_height);

    return Vec2(screen_x, screen_y);
}

void UIWorldCanvas::update_for_camera(const render::CameraData& camera,
                                        uint32_t screen_width,
                                        uint32_t screen_height) {
    // Compute distance from camera to world position
    Vec3 to_canvas = m_world_position - camera.position;
    m_current_distance = glm::length(to_canvas);

    // Distance culling
    if (m_current_distance > m_max_distance) {
        m_visible_in_frustum = false;
        m_distance_alpha = 0.0f;
        return;
    }

    // Distance-based fade
    if (m_current_distance > m_max_distance - m_fade_range) {
        float fade_start = m_max_distance - m_fade_range;
        m_distance_alpha = 1.0f - (m_current_distance - fade_start) / m_fade_range;
        m_distance_alpha = std::clamp(m_distance_alpha, 0.0f, 1.0f);
    } else {
        m_distance_alpha = 1.0f;
    }

    // Compute screen-space scale
    if (m_constant_screen_size && m_reference_distance > 0.0f) {
        float scale_factor = m_current_distance / m_reference_distance;
        m_computed_scale = m_world_scale * scale_factor;
        m_computed_scale = std::clamp(m_computed_scale, m_min_scale, m_max_scale);
    } else {
        m_computed_scale = m_world_scale;
    }

    // Project world position to screen
    m_screen_position = project_to_screen(m_world_position, camera.view_projection,
                                           screen_width, screen_height, &m_behind_camera);

    if (m_behind_camera) {
        m_visible_in_frustum = false;
        return;
    }

    // Apply screen offset
    m_screen_position += m_screen_offset;

    // Check if on screen (with some margin)
    float margin = 50.0f;
    float half_w = get_width() * m_computed_scale * 0.5f;
    float half_h = get_height() * m_computed_scale * 0.5f;

    bool on_screen =
        m_screen_position.x + half_w >= -margin &&
        m_screen_position.x - half_w <= static_cast<float>(screen_width) + margin &&
        m_screen_position.y + half_h >= -margin &&
        m_screen_position.y - half_h <= static_cast<float>(screen_height) + margin;

    m_visible_in_frustum = on_screen;

    // Note: The actual rendering position adjustment (centering the canvas at screen position)
    // should be done by the caller when positioning the canvas for render
}

} // namespace engine::ui
