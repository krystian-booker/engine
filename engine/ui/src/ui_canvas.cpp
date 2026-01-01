#include <engine/ui/ui_canvas.hpp>

namespace engine::ui {

UICanvas::UICanvas() = default;
UICanvas::~UICanvas() = default;

void UICanvas::set_root(std::unique_ptr<UIElement> root) {
    m_root = std::move(root);
    m_layout_dirty = true;
}

void UICanvas::set_size(uint32_t width, uint32_t height) {
    if (m_width != width || m_height != height) {
        m_width = width;
        m_height = height;
        m_layout_dirty = true;
    }
}

void UICanvas::set_reference_resolution(uint32_t width, uint32_t height) {
    m_reference_width = width;
    m_reference_height = height;
    m_layout_dirty = true;
}

void UICanvas::update(float dt, const UIInputState& input) {
    if (!m_enabled || !m_root) return;

    // Layout if needed
    if (m_layout_dirty) {
        layout_root();
    }

    // Track hovered element
    UIElement* new_hovered = m_root->find_element_at(input.mouse_position);
    if (new_hovered != m_hovered_element) {
        m_hovered_element = new_hovered;
    }

    // Update element tree
    m_root->update(dt, input);
}

void UICanvas::render(UIRenderContext& ctx) {
    if (!m_enabled || !m_root) return;

    m_root->render(ctx);
}

void UICanvas::set_focused_element(UIElement* element) {
    if (m_focused_element == element) return;

    if (m_focused_element) {
        m_focused_element->release_focus();
    }

    m_focused_element = element;

    if (m_focused_element) {
        m_focused_element->request_focus();
    }
}

UIElement* UICanvas::find_element_at(Vec2 point) {
    if (!m_root) return nullptr;
    return m_root->find_element_at(point);
}

void UICanvas::layout_root() {
    if (!m_root) return;

    // Calculate scale based on scale mode
    float scale_x = 1.0f;
    float scale_y = 1.0f;

    if (m_scale_mode == ScaleMode::ScaleWithScreen) {
        scale_x = static_cast<float>(m_width) / m_reference_width;
        scale_y = static_cast<float>(m_height) / m_reference_height;
    }

    // Create root bounds
    Rect root_bounds(0, 0,
                     static_cast<float>(m_width) / scale_x,
                     static_cast<float>(m_height) / scale_y);

    m_root->layout(root_bounds);
    m_layout_dirty = false;
}

} // namespace engine::ui
