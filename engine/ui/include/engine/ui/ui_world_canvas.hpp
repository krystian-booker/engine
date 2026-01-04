#pragma once

#include <engine/ui/ui_canvas.hpp>
#include <engine/core/math.hpp>

namespace engine::render {
    struct CameraData;
}

namespace engine::ui {

// Billboard behavior for world-space UI
enum class WorldCanvasBillboard : uint8_t {
    None,           // Fixed orientation in world space
    FaceCamera,     // Always face camera (full billboard)
    FaceCamera_Y    // Face camera but locked to Y axis (cylindrical)
};

// World-space canvas that renders UI at a 3D position
// Use for health bars above characters, interaction prompts, etc.
class UIWorldCanvas : public UICanvas {
public:
    UIWorldCanvas();
    ~UIWorldCanvas() override = default;

    // World-space transform
    void set_world_position(const Vec3& position);
    Vec3 get_world_position() const { return m_world_position; }

    void set_world_rotation(const Quat& rotation);
    Quat get_world_rotation() const { return m_world_rotation; }

    void set_world_scale(float scale);
    float get_world_scale() const { return m_world_scale; }

    // Billboard mode
    void set_billboard(WorldCanvasBillboard mode) { m_billboard = mode; }
    WorldCanvasBillboard get_billboard() const { return m_billboard; }

    // Keep same screen size regardless of distance
    void set_constant_screen_size(bool constant) { m_constant_screen_size = constant; }
    bool get_constant_screen_size() const { return m_constant_screen_size; }

    // Reference distance for constant screen size (size at this distance = base size)
    void set_reference_distance(float distance) { m_reference_distance = distance; }
    float get_reference_distance() const { return m_reference_distance; }

    // Minimum/maximum scale multipliers for constant screen size
    void set_min_scale(float scale) { m_min_scale = scale; }
    float get_min_scale() const { return m_min_scale; }

    void set_max_scale(float scale) { m_max_scale = scale; }
    float get_max_scale() const { return m_max_scale; }

    // Maximum render distance (beyond this, canvas is culled)
    void set_max_distance(float distance) { m_max_distance = distance; }
    float get_max_distance() const { return m_max_distance; }

    // Fade range (starts fading at max_distance - fade_range)
    void set_fade_range(float range) { m_fade_range = range; }
    float get_fade_range() const { return m_fade_range; }

    // Offset from world position (in screen space after projection)
    void set_screen_offset(Vec2 offset) { m_screen_offset = offset; }
    Vec2 get_screen_offset() const { return m_screen_offset; }

    // Update with camera data (computes screen position, visibility, alpha)
    void update_for_camera(const render::CameraData& camera,
                            uint32_t screen_width, uint32_t screen_height);

    // Get computed results after update_for_camera()
    Vec2 get_screen_position() const { return m_screen_position; }
    bool is_visible() const { return m_visible_in_frustum && is_enabled(); }
    float get_distance_alpha() const { return m_distance_alpha; }
    float get_current_distance() const { return m_current_distance; }
    float get_computed_scale() const { return m_computed_scale; }

    // Check if behind camera
    bool is_behind_camera() const { return m_behind_camera; }

    // Static helper: Project world position to screen coordinates
    static Vec2 project_to_screen(const Vec3& world_pos,
                                   const Mat4& view_projection,
                                   uint32_t screen_width,
                                   uint32_t screen_height,
                                   bool* out_behind_camera = nullptr);

private:
    // World transform
    Vec3 m_world_position{0.0f};
    Quat m_world_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    float m_world_scale = 1.0f;

    // Billboard settings
    WorldCanvasBillboard m_billboard = WorldCanvasBillboard::FaceCamera;

    // Screen size settings
    bool m_constant_screen_size = false;
    float m_reference_distance = 10.0f;
    float m_min_scale = 0.5f;
    float m_max_scale = 2.0f;

    // Distance culling/fading
    float m_max_distance = 100.0f;
    float m_fade_range = 10.0f;

    // Screen offset (pixels)
    Vec2 m_screen_offset{0.0f};

    // Computed each frame
    Vec2 m_screen_position{0.0f};
    bool m_visible_in_frustum = true;
    bool m_behind_camera = false;
    float m_distance_alpha = 1.0f;
    float m_current_distance = 0.0f;
    float m_computed_scale = 1.0f;
};

} // namespace engine::ui
