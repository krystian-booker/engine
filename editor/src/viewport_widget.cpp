#include "viewport_widget.hpp"
#include <engine/scene/transform.hpp>
#include <engine/scene/systems.hpp>
#include <engine/scene/render_components.hpp>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QElapsedTimer>
#include <cmath>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace editor {

ViewportWidget::ViewportWidget(EditorState* state, QWidget* parent)
    : QWidget(parent)
    , m_state(state)
{
    // Set up for native rendering
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    // Update camera position from spherical coordinates
    m_camera_pos = m_camera_target + Vec3{
        m_camera_distance * std::cos(m_camera_pitch) * std::sin(m_camera_yaw),
        m_camera_distance * std::sin(m_camera_pitch),
        m_camera_distance * std::cos(m_camera_pitch) * std::cos(m_camera_yaw)
    };

    // Set up render timer (60 FPS)
    m_render_timer = new QTimer(this);
    connect(m_render_timer, &QTimer::timeout, this, &ViewportWidget::render_frame);
    m_render_timer->start(16); // ~60 FPS
}

ViewportWidget::~ViewportWidget() {
    m_render_timer->stop();
}

void* ViewportWidget::native_handle() {
#ifdef _WIN32
    return reinterpret_cast<void*>(winId());
#else
    return reinterpret_cast<void*>(winId());
#endif
}

void ViewportWidget::set_camera_entity(engine::scene::Entity camera) {
    m_camera_entity = camera;
}

void ViewportWidget::focus_selection() {
    if (!m_state || m_state->selection().empty() || !m_state->world()) {
        return;
    }

    auto entity = m_state->primary_selection();
    auto* transform = m_state->world()->try_get<engine::scene::WorldTransform>(entity);
    if (transform) {
        m_camera_target = transform->position();
        m_camera_pos = m_camera_target + Vec3{
            m_camera_distance * std::cos(m_camera_pitch) * std::sin(m_camera_yaw),
            m_camera_distance * std::sin(m_camera_pitch),
            m_camera_distance * std::cos(m_camera_pitch) * std::cos(m_camera_yaw)
        };
    }
}

void ViewportWidget::render_frame() {
    if (!m_state || !m_state->renderer()) {
        return;
    }

    auto* renderer = m_state->renderer();
    auto* world = m_state->world();

    // Calculate delta time
    static QElapsedTimer timer;
    if (!timer.isValid()) {
        timer.start();
        m_last_frame_time = 0;
    }
    qint64 current = timer.elapsed();
    float dt = (current - m_last_frame_time) / 1000.0f;
    m_last_frame_time = current;

    // Update editor camera if not using scene camera
    if (m_camera_entity == engine::scene::NullEntity) {
        update_editor_camera(dt);
    }

    // Begin frame
    renderer->begin_frame();
    renderer->clear(0x303030ff);

    // Set up camera matrices
    Mat4 view, proj;
    float aspect = static_cast<float>(width()) / static_cast<float>(std::max(1, height()));

    if (m_camera_entity != engine::scene::NullEntity && world) {
        // Use scene camera
        auto* cam_transform = world->try_get<engine::scene::WorldTransform>(m_camera_entity);
        auto* camera = world->try_get<engine::scene::Camera>(m_camera_entity);
        if (cam_transform && camera) {
            Vec3 pos = cam_transform->position();
            Quat rot = cam_transform->rotation();
            Vec3 forward = rot * Vec3{0.0f, 0.0f, -1.0f};
            Vec3 up = rot * Vec3{0.0f, 1.0f, 0.0f};
            view = glm::lookAt(pos, pos + forward, up);
            proj = glm::perspective(glm::radians(camera->fov), aspect, camera->near_plane, camera->far_plane);
        }
    } else {
        // Use editor camera
        view = glm::lookAt(m_camera_pos, m_camera_target, Vec3{0.0f, 1.0f, 0.0f});
        proj = glm::perspective(glm::radians(m_camera_fov), aspect, m_camera_near, m_camera_far);
    }

    renderer->set_camera(view, proj);

    // Draw grid
    if (m_show_grid) {
        draw_grid();
    }

    // Render scene
    if (world) {
        // Run PreRender phase systems (includes transform_system)
        if (auto* scheduler = m_state->scheduler()) {
            scheduler->run(*world, 0.0, engine::scene::Phase::PreRender);
        }

        auto render_view = world->view<engine::scene::WorldTransform, engine::scene::MeshRenderer>();
        render_view.each([&](auto entity, auto& transform, auto& mesh_renderer) {
            if (!mesh_renderer.visible) return;

            engine::render::DrawCall call;
            call.mesh = engine::render::MeshHandle{mesh_renderer.mesh.id};
            call.material = engine::render::MaterialHandle{mesh_renderer.material.id};
            call.transform = transform.matrix;
            renderer->queue_draw(call);
        });
    }

    // Draw gizmo for selection
    if (m_show_gizmo && !m_state->selection().empty()) {
        draw_gizmo();
    }

    // Flush and end frame
    renderer->flush();
    renderer->end_frame();
}

void ViewportWidget::paintEvent(QPaintEvent* /*event*/) {
    // Rendering handled by render_frame()
}

void ViewportWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);

    if (m_state && m_state->renderer()) {
        const float dpr = devicePixelRatioF();
        const uint32_t w = static_cast<uint32_t>(event->size().width() * dpr);
        const uint32_t h = static_cast<uint32_t>(event->size().height() * dpr);
        m_state->renderer()->resize(w, h);
    }

    emit viewport_resized(event->size().width(), event->size().height());
}

void ViewportWidget::mousePressEvent(QMouseEvent* event) {
    m_last_mouse_pos = event->pos();
    m_pressed_buttons = event->buttons();
    m_mouse_dragging = true;

    // Alt + LMB = orbit, Alt + MMB = pan, Alt + RMB = zoom
    bool alt_pressed = event->modifiers() & Qt::AltModifier;

    if (alt_pressed) {
        if (event->button() == Qt::LeftButton) {
            m_orbit_mode = true;
            m_pan_mode = false;
        } else if (event->button() == Qt::MiddleButton) {
            m_orbit_mode = false;
            m_pan_mode = true;
        }
    } else if (event->button() == Qt::MiddleButton) {
        // MMB alone for pan
        m_pan_mode = true;
    } else if (event->button() == Qt::RightButton) {
        // RMB for orbit
        m_orbit_mode = true;
    } else if (event->button() == Qt::LeftButton) {
        // Left click for selection
        auto picked = pick_entity(event->pos().x(), event->pos().y());
        if (event->modifiers() & Qt::ControlModifier) {
            m_state->toggle_selection(picked);
        } else {
            m_state->select(picked);
        }
        emit entity_picked(picked);
    }

    setFocus();
}

void ViewportWidget::mouseReleaseEvent(QMouseEvent* event) {
    m_pressed_buttons = event->buttons();

    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) {
        m_orbit_mode = false;
    }
    if (event->button() == Qt::MiddleButton) {
        m_pan_mode = false;
    }

    if (m_pressed_buttons == Qt::NoButton) {
        m_mouse_dragging = false;
    }
}

void ViewportWidget::mouseMoveEvent(QMouseEvent* event) {
    QPoint delta = event->pos() - m_last_mouse_pos;
    m_last_mouse_pos = event->pos();

    if (m_orbit_mode) {
        handle_camera_orbit(delta);
    } else if (m_pan_mode) {
        handle_camera_pan(delta);
    }
}

void ViewportWidget::wheelEvent(QWheelEvent* event) {
    float delta = event->angleDelta().y() / 120.0f;
    handle_camera_zoom(delta);
}

void ViewportWidget::keyPressEvent(QKeyEvent* event) {
    // Handle camera movement with WASD
    switch (event->key()) {
        case Qt::Key_F:
            focus_selection();
            break;
        case Qt::Key_W:
        case Qt::Key_S:
        case Qt::Key_A:
        case Qt::Key_D:
        case Qt::Key_Q:
        case Qt::Key_E:
            // Handled in update_editor_camera
            break;
        default:
            QWidget::keyPressEvent(event);
            break;
    }
}

void ViewportWidget::keyReleaseEvent(QKeyEvent* event) {
    QWidget::keyReleaseEvent(event);
}

void ViewportWidget::update_editor_camera(float /*dt*/) {
    // Camera position is updated through orbit/pan/zoom handlers
}

void ViewportWidget::handle_camera_orbit(const QPoint& delta) {
    const float sensitivity = 0.01f;

    m_camera_yaw -= delta.x() * sensitivity;
    m_camera_pitch -= delta.y() * sensitivity;

    // Clamp pitch to avoid gimbal lock
    m_camera_pitch = glm::clamp(m_camera_pitch, -glm::half_pi<float>() + 0.1f,
                                                  glm::half_pi<float>() - 0.1f);

    // Update camera position
    m_camera_pos = m_camera_target + Vec3{
        m_camera_distance * std::cos(m_camera_pitch) * std::sin(m_camera_yaw),
        m_camera_distance * std::sin(m_camera_pitch),
        m_camera_distance * std::cos(m_camera_pitch) * std::cos(m_camera_yaw)
    };
}

void ViewportWidget::handle_camera_pan(const QPoint& delta) {
    const float sensitivity = 0.01f * m_camera_distance;

    Vec3 forward = glm::normalize(m_camera_target - m_camera_pos);
    Vec3 right = glm::normalize(glm::cross(forward, Vec3{0.0f, 1.0f, 0.0f}));
    Vec3 up = glm::cross(right, forward);

    Vec3 offset = right * (-delta.x() * sensitivity) + up * (delta.y() * sensitivity);
    m_camera_pos += offset;
    m_camera_target += offset;
}

void ViewportWidget::handle_camera_zoom(float delta) {
    const float zoom_speed = 0.1f;

    m_camera_distance *= (1.0f - delta * zoom_speed);
    m_camera_distance = glm::clamp(m_camera_distance, 0.5f, 500.0f);

    // Update camera position
    m_camera_pos = m_camera_target + Vec3{
        m_camera_distance * std::cos(m_camera_pitch) * std::sin(m_camera_yaw),
        m_camera_distance * std::sin(m_camera_pitch),
        m_camera_distance * std::cos(m_camera_pitch) * std::cos(m_camera_yaw)
    };
}

engine::scene::Entity ViewportWidget::pick_entity(int /*x*/, int /*y*/) {
    // TODO: Implement ray-based picking
    // For now, return null (no selection)
    return engine::scene::NullEntity;
}

void ViewportWidget::draw_grid() {
    // TODO: Draw grid lines using debug draw
    // This will be implemented with Phase 11 debug drawing
}

void ViewportWidget::draw_gizmo() {
    // TODO: Draw transform gizmo for selected entity
    // This will be implemented with Phase 11 debug drawing
}

} // namespace editor
