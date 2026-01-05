#pragma once

#include "editor_state.hpp"
#include <engine/render/renderer.hpp>
#include <engine/render/render_to_texture.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/entity.hpp>
#include <engine/core/math.hpp>
#include <QWidget>
#include <QTimer>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <memory>

namespace editor {

using namespace engine::core;

// Game View widget - displays what the active game camera sees
class GameViewWidget : public QWidget {
    Q_OBJECT

public:
    explicit GameViewWidget(EditorState* state, QWidget* parent = nullptr);
    ~GameViewWidget() override;

    // Initialize RTT (must be called after renderer is initialized)
    void init_rtt();
    void shutdown_rtt();

    // Rendering
    void render_frame();

    // Stats overlay
    bool show_stats() const { return m_show_stats; }
    void set_show_stats(bool show) { m_show_stats = show; }

    // Aspect ratio
    bool lock_aspect_ratio() const { return m_lock_aspect_ratio; }
    void set_lock_aspect_ratio(bool lock);

signals:
    void viewport_resized(int width, int height);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void on_active_camera_changed(engine::scene::Entity camera);
    void on_resolution_changed(int index);
    void on_aspect_ratio_toggled(bool checked);
    void on_stats_toggled(bool checked);

private:
    void setup_ui();
    void setup_toolbar();
    void update_placeholder_visibility();
    void create_render_target(uint32_t width, uint32_t height);
    void destroy_render_target();
    void render_camera_view();
    void draw_stats_overlay(QPainter& painter);
    QSize calculate_viewport_size() const;

    EditorState* m_state;

    // UI components
    QVBoxLayout* m_main_layout;
    QWidget* m_toolbar;
    QComboBox* m_resolution_combo;
    QCheckBox* m_aspect_ratio_checkbox;
    QPushButton* m_stats_button;
    QWidget* m_render_area;
    QLabel* m_placeholder_label;

    // Current active camera
    engine::scene::Entity m_current_camera = engine::scene::NullEntity;

    // RTT target
    engine::render::RTTTarget m_rtt_target;
    bool m_rtt_initialized = false;

    // Render timer
    QTimer* m_render_timer;

    // Viewport settings
    bool m_show_stats = false;
    bool m_lock_aspect_ratio = false;
    float m_aspect_ratio = 16.0f / 9.0f;

    // Resolution presets (width, height) - 0,0 means "Native" (match widget size)
    struct ResolutionPreset {
        QString name;
        uint32_t width;
        uint32_t height;
    };
    static const std::vector<ResolutionPreset> s_resolution_presets;
    int m_current_resolution_index = 0;

    // Render target dimensions
    uint32_t m_rtt_width = 0;
    uint32_t m_rtt_height = 0;

    // Stats tracking
    float m_fps = 0.0f;
    qint64 m_last_frame_time = 0;
    int m_frame_count = 0;
    float m_fps_update_timer = 0.0f;

    // Rendered image for display
    QImage m_rendered_image;
};

} // namespace editor
