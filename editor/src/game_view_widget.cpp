#include "game_view_widget.hpp"
#include <engine/scene/transform.hpp>
#include <engine/scene/render_components.hpp>
#include <QPainter>
#include <QResizeEvent>
#include <QElapsedTimer>
#include <QToolBar>

namespace editor {

// Resolution presets
const std::vector<GameViewWidget::ResolutionPreset> GameViewWidget::s_resolution_presets = {
    {"Native", 0, 0},
    {"1920x1080", 1920, 1080},
    {"1280x720", 1280, 720},
    {"640x360", 640, 360},
};

GameViewWidget::GameViewWidget(EditorState* state, QWidget* parent)
    : QWidget(parent)
    , m_state(state)
{
    setup_ui();

    // Connect to EditorState signals
    if (m_state) {
        connect(m_state, &EditorState::active_camera_changed,
                this, &GameViewWidget::on_active_camera_changed);

        // Initialize with current camera
        m_current_camera = m_state->active_game_camera();
    }

    // Set up render timer (60 FPS)
    m_render_timer = new QTimer(this);
    connect(m_render_timer, &QTimer::timeout, this, &GameViewWidget::render_frame);
    m_render_timer->start(16);

    update_placeholder_visibility();
}

GameViewWidget::~GameViewWidget() {
    m_render_timer->stop();
    shutdown_rtt();
}

void GameViewWidget::setup_ui() {
    m_main_layout = new QVBoxLayout(this);
    m_main_layout->setContentsMargins(0, 0, 0, 0);
    m_main_layout->setSpacing(0);

    setup_toolbar();

    // Create stacked area for render/placeholder
    m_render_area = new QWidget(this);
    m_render_area->setStyleSheet("background-color: #303030;");

    // Placeholder label
    m_placeholder_label = new QLabel("No Active Camera", m_render_area);
    m_placeholder_label->setAlignment(Qt::AlignCenter);
    m_placeholder_label->setStyleSheet(
        "QLabel { color: #888888; font-size: 16px; background-color: transparent; }");

    // Layout for render area to center placeholder
    auto* render_layout = new QVBoxLayout(m_render_area);
    render_layout->addWidget(m_placeholder_label, 0, Qt::AlignCenter);

    m_main_layout->addWidget(m_render_area, 1);
}

void GameViewWidget::setup_toolbar() {
    m_toolbar = new QWidget(this);
    m_toolbar->setFixedHeight(28);
    m_toolbar->setStyleSheet(
        "QWidget { background-color: #3c3c3c; border-bottom: 1px solid #2a2a2a; }"
        "QComboBox { background-color: #505050; color: #cccccc; border: 1px solid #606060; "
        "           border-radius: 2px; padding: 2px 6px; min-width: 80px; }"
        "QComboBox:hover { background-color: #5a5a5a; }"
        "QComboBox::drop-down { border: none; }"
        "QCheckBox { color: #cccccc; spacing: 4px; }"
        "QCheckBox::indicator { width: 14px; height: 14px; }"
        "QPushButton { background-color: #505050; color: #cccccc; border: 1px solid #606060; "
        "              border-radius: 2px; padding: 2px 8px; }"
        "QPushButton:hover { background-color: #5a5a5a; }"
        "QPushButton:checked { background-color: #4080c0; }"
    );

    auto* toolbar_layout = new QHBoxLayout(m_toolbar);
    toolbar_layout->setContentsMargins(4, 2, 4, 2);
    toolbar_layout->setSpacing(8);

    // Resolution dropdown
    auto* res_label = new QLabel("Resolution:", m_toolbar);
    res_label->setStyleSheet("QLabel { color: #aaaaaa; background: transparent; border: none; }");
    toolbar_layout->addWidget(res_label);

    m_resolution_combo = new QComboBox(m_toolbar);
    for (const auto& preset : s_resolution_presets) {
        m_resolution_combo->addItem(preset.name);
    }
    connect(m_resolution_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GameViewWidget::on_resolution_changed);
    toolbar_layout->addWidget(m_resolution_combo);

    toolbar_layout->addSpacing(16);

    // Aspect ratio checkbox
    m_aspect_ratio_checkbox = new QCheckBox("Lock Aspect", m_toolbar);
    m_aspect_ratio_checkbox->setChecked(m_lock_aspect_ratio);
    connect(m_aspect_ratio_checkbox, &QCheckBox::toggled,
            this, &GameViewWidget::on_aspect_ratio_toggled);
    toolbar_layout->addWidget(m_aspect_ratio_checkbox);

    toolbar_layout->addSpacing(16);

    // Stats button
    m_stats_button = new QPushButton("Stats", m_toolbar);
    m_stats_button->setCheckable(true);
    m_stats_button->setChecked(m_show_stats);
    connect(m_stats_button, &QPushButton::toggled,
            this, &GameViewWidget::on_stats_toggled);
    toolbar_layout->addWidget(m_stats_button);

    toolbar_layout->addStretch();

    m_main_layout->addWidget(m_toolbar);
}

void GameViewWidget::init_rtt() {
    if (!m_state || !m_state->renderer()) {
        return;
    }

    auto size = calculate_viewport_size();
    create_render_target(size.width(), size.height());
}

void GameViewWidget::shutdown_rtt() {
    destroy_render_target();
}

void GameViewWidget::create_render_target(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        return;
    }

    // Clean up existing target
    destroy_render_target();

    auto& rtt_system = engine::render::get_rtt_system();
    m_rtt_target = rtt_system.create_target(width, height,
        engine::render::TextureFormat::RGBA8, true);

    m_rtt_width = width;
    m_rtt_height = height;
    m_rtt_initialized = m_rtt_target.valid;

    // Pre-allocate image buffer for display
    m_rendered_image = QImage(width, height, QImage::Format_RGBA8888);
}

void GameViewWidget::destroy_render_target() {
    if (m_rtt_initialized) {
        auto& rtt_system = engine::render::get_rtt_system();
        rtt_system.destroy_target(m_rtt_target);
        m_rtt_initialized = false;
        m_rtt_width = 0;
        m_rtt_height = 0;
    }
}

void GameViewWidget::render_frame() {
    if (!m_state || !m_state->renderer() || !isVisible()) {
        return;
    }

    // Calculate delta time
    static QElapsedTimer timer;
    if (!timer.isValid()) {
        timer.start();
        m_last_frame_time = 0;
    }
    qint64 current = timer.elapsed();
    float dt = (current - m_last_frame_time) / 1000.0f;
    m_last_frame_time = current;

    // FPS tracking
    m_frame_count++;
    m_fps_update_timer += dt;
    if (m_fps_update_timer >= 0.5f) {
        m_fps = m_frame_count / m_fps_update_timer;
        m_frame_count = 0;
        m_fps_update_timer = 0.0f;
    }

    // Render camera view if we have an active camera
    if (m_current_camera != engine::scene::NullEntity && m_rtt_initialized) {
        render_camera_view();
    }

    // Trigger repaint
    m_render_area->update();
}

void GameViewWidget::render_camera_view() {
    // RTT rendering implementation will be in Phase 4
    // For now, just clear to a different color to show it's working
    if (!m_rtt_initialized || !m_state || !m_state->world()) {
        return;
    }

    auto* world = m_state->world();
    auto* renderer = m_state->renderer();

    // Get camera transform and component
    auto* cam_transform = world->try_get<engine::scene::WorldTransform>(m_current_camera);
    auto* camera = world->try_get<engine::scene::Camera>(m_current_camera);

    if (!cam_transform || !camera) {
        return;
    }

    // Calculate view and projection matrices
    Vec3 pos = cam_transform->position();
    Quat rot = cam_transform->rotation();
    Vec3 forward = rot * Vec3{0.0f, 0.0f, -1.0f};
    Vec3 up = rot * Vec3{0.0f, 1.0f, 0.0f};

    float aspect = static_cast<float>(m_rtt_width) /
                   static_cast<float>(std::max(1u, m_rtt_height));

    Mat4 view = glm::lookAt(pos, pos + forward, up);
    Mat4 proj;
    if (camera->orthographic) {
        float w = camera->ortho_size * aspect;
        float h = camera->ortho_size;
        proj = glm::ortho(-w, w, -h, h, camera->near_plane, camera->far_plane);
    } else {
        proj = glm::perspective(glm::radians(camera->fov), aspect,
                               camera->near_plane, camera->far_plane);
    }

    // TODO: Configure view for RTT rendering
    // This will be implemented in Phase 4 with proper bgfx view setup
    // For now, we'll just render to the backbuffer through the main viewport
}

void GameViewWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);

    // Draw toolbar area background (already handled by stylesheet)

    // Draw render area
    QRect render_rect = m_render_area->geometry();

    if (m_current_camera == engine::scene::NullEntity) {
        // No camera - placeholder is shown via label
        painter.fillRect(render_rect, QColor(0x30, 0x30, 0x30));
    } else if (m_rtt_initialized && !m_rendered_image.isNull()) {
        // Draw rendered image (scaled to fit if needed)
        QRect target_rect = render_rect;

        if (m_lock_aspect_ratio) {
            // Calculate letterbox/pillarbox rect
            float widget_aspect = static_cast<float>(render_rect.width()) / render_rect.height();
            if (widget_aspect > m_aspect_ratio) {
                // Pillarbox
                int new_width = static_cast<int>(render_rect.height() * m_aspect_ratio);
                target_rect.setLeft(render_rect.left() + (render_rect.width() - new_width) / 2);
                target_rect.setWidth(new_width);
            } else {
                // Letterbox
                int new_height = static_cast<int>(render_rect.width() / m_aspect_ratio);
                target_rect.setTop(render_rect.top() + (render_rect.height() - new_height) / 2);
                target_rect.setHeight(new_height);
            }

            // Fill letterbox/pillarbox areas with black
            painter.fillRect(render_rect, Qt::black);
        }

        painter.drawImage(target_rect, m_rendered_image);
    } else {
        // Camera exists but RTT not ready - show loading or dark background
        painter.fillRect(render_rect, QColor(0x30, 0x30, 0x30));
    }

    // Draw stats overlay
    if (m_show_stats && m_current_camera != engine::scene::NullEntity) {
        draw_stats_overlay(painter);
    }
}

void GameViewWidget::draw_stats_overlay(QPainter& painter) {
    QRect render_rect = m_render_area->geometry();

    // Semi-transparent background
    painter.fillRect(render_rect.left() + 5, render_rect.top() + 5, 150, 60,
                     QColor(0, 0, 0, 180));

    painter.setPen(Qt::white);
    painter.setFont(QFont("Consolas", 10));

    int y = render_rect.top() + 20;
    painter.drawText(render_rect.left() + 10, y, QString("FPS: %1").arg(m_fps, 0, 'f', 1));
    y += 16;
    painter.drawText(render_rect.left() + 10, y,
                     QString("Resolution: %1x%2").arg(m_rtt_width).arg(m_rtt_height));
    y += 16;
    // TODO: Add draw call count, triangle count, etc.
}

void GameViewWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);

    // Update placeholder position
    if (m_placeholder_label) {
        m_placeholder_label->setGeometry(m_render_area->rect());
    }

    // Recreate RTT if using native resolution
    if (m_current_resolution_index == 0 && m_rtt_initialized) {
        auto size = calculate_viewport_size();
        if (size.width() != static_cast<int>(m_rtt_width) ||
            size.height() != static_cast<int>(m_rtt_height)) {
            create_render_target(size.width(), size.height());
        }
    }

    emit viewport_resized(event->size().width(), event->size().height());
}

QSize GameViewWidget::calculate_viewport_size() const {
    const auto& preset = s_resolution_presets[m_current_resolution_index];

    if (preset.width == 0 || preset.height == 0) {
        // Native resolution - use render area size
        QSize area_size = m_render_area ? m_render_area->size() : size();
        return QSize(std::max(1, area_size.width()), std::max(1, area_size.height()));
    }

    return QSize(preset.width, preset.height);
}

void GameViewWidget::update_placeholder_visibility() {
    bool has_camera = m_current_camera != engine::scene::NullEntity;
    m_placeholder_label->setVisible(!has_camera);
}

void GameViewWidget::on_active_camera_changed(engine::scene::Entity camera) {
    m_current_camera = camera;
    update_placeholder_visibility();

    // Reinitialize RTT if camera state changed
    if (m_current_camera != engine::scene::NullEntity && !m_rtt_initialized) {
        init_rtt();
    }

    update();
}

void GameViewWidget::on_resolution_changed(int index) {
    if (index < 0 || index >= static_cast<int>(s_resolution_presets.size())) {
        return;
    }

    m_current_resolution_index = index;
    const auto& preset = s_resolution_presets[index];

    // Update aspect ratio for lock feature
    if (preset.width > 0 && preset.height > 0) {
        m_aspect_ratio = static_cast<float>(preset.width) / preset.height;
    }

    // Recreate render target with new size
    auto size = calculate_viewport_size();
    create_render_target(size.width(), size.height());

    update();
}

void GameViewWidget::on_aspect_ratio_toggled(bool checked) {
    m_lock_aspect_ratio = checked;
    update();
}

void GameViewWidget::on_stats_toggled(bool checked) {
    m_show_stats = checked;
    update();
}

void GameViewWidget::set_lock_aspect_ratio(bool lock) {
    m_lock_aspect_ratio = lock;
    m_aspect_ratio_checkbox->setChecked(lock);
}

} // namespace editor
