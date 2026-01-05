#pragma once

#include "editor_state.hpp"
#include <engine/render/renderer.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/entity.hpp>
#include <engine/core/math.hpp>
#include <QWidget>
#include <QTimer>
#include <memory>
#include <set>

namespace editor {

using namespace engine::core;

// Viewport widget that renders the engine scene to a Qt widget
class ViewportWidget : public QWidget {
    Q_OBJECT

public:
    explicit ViewportWidget(EditorState* state, QWidget* parent = nullptr);
    ~ViewportWidget() override;

    // Get native window handle for BGFX
    void* native_handle();

    // Camera control
    void set_camera_entity(engine::scene::Entity camera);
    engine::scene::Entity camera_entity() const { return m_camera_entity; }

    // Editor camera (when no scene camera is set)
    Vec3 camera_position() const { return m_camera_pos; }
    void set_camera_position(const Vec3& pos) { m_camera_pos = pos; }

    Vec3 camera_target() const { return m_camera_target; }
    void set_camera_target(const Vec3& target) { m_camera_target = target; }

    // Focus on selected entity
    void focus_selection();

    // Rendering
    void render_frame();

    // View ID for multi-viewport support
    void set_view_id(uint16_t id) { m_view_id = id; }
    uint16_t view_id() const { return m_view_id; }

    // Gizmo settings
    bool show_grid() const { return m_show_grid; }
    void set_show_grid(bool show) { m_show_grid = show; }

    bool show_gizmo() const { return m_show_gizmo; }
    void set_show_gizmo(bool show) { m_show_gizmo = show; }

    // Check if fly mode is active (RMB held for camera control)
    bool is_fly_mode_active() const { return m_fly_mode; }

signals:
    void entity_picked(engine::scene::Entity entity);
    void viewport_resized(int width, int height);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

    QPaintEngine* paintEngine() const override { return nullptr; }

private:
    void update_editor_camera(float dt);
    void handle_camera_orbit(const QPoint& delta);
    void handle_camera_pan(const QPoint& delta);
    void handle_camera_zoom(float delta);
    engine::scene::Entity pick_entity(int x, int y);
    void draw_grid();
    void draw_gizmo();

    EditorState* m_state;
    engine::scene::Entity m_camera_entity = engine::scene::NullEntity;

    // Editor camera state
    Vec3 m_camera_pos{0.0f, 5.0f, 10.0f};
    Vec3 m_camera_target{0.0f, 0.0f, 0.0f};
    float m_camera_distance = 10.0f;
    float m_camera_yaw = 0.0f;
    float m_camera_pitch = -0.3f;
    float m_camera_fov = 60.0f;
    float m_camera_near = 0.1f;
    float m_camera_far = 1000.0f;

    // Input state
    QPoint m_last_mouse_pos;
    bool m_mouse_dragging = false;
    Qt::MouseButtons m_pressed_buttons;
    bool m_orbit_mode = false;
    bool m_pan_mode = false;
    bool m_fly_mode = false;  // WASD fly camera active (when RMB held)
    std::set<int> m_keys_pressed;  // Currently pressed keys
    float m_fly_speed = 5.0f;  // Fly camera movement speed

    // Rendering state
    uint16_t m_view_id = 0;
    bool m_show_grid = true;
    bool m_show_gizmo = true;

    // Frame timing
    QTimer* m_render_timer;
    qint64 m_last_frame_time = 0;
};

} // namespace editor
