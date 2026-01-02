#include <engine/ui/ui_element.hpp>
#include <engine/ui/ui_renderer.hpp>
#include <algorithm>

namespace engine::ui {

UIElement::UIElement() = default;
UIElement::~UIElement() = default;

void UIElement::add_class(const std::string& class_name) {
    if (std::find(m_classes.begin(), m_classes.end(), class_name) == m_classes.end()) {
        m_classes.push_back(class_name);
        mark_dirty();
    }
}

void UIElement::remove_class(const std::string& class_name) {
    auto it = std::find(m_classes.begin(), m_classes.end(), class_name);
    if (it != m_classes.end()) {
        m_classes.erase(it);
        mark_dirty();
    }
}

bool UIElement::has_class(const std::string& class_name) const {
    return std::find(m_classes.begin(), m_classes.end(), class_name) != m_classes.end();
}

void UIElement::add_child(std::unique_ptr<UIElement> child) {
    if (!child) return;

    child->m_parent = this;
    m_children.push_back(std::move(child));
    mark_layout_dirty();
}

void UIElement::remove_child(UIElement* child) {
    if (!child) return;

    auto it = std::find_if(m_children.begin(), m_children.end(),
        [child](const std::unique_ptr<UIElement>& ptr) { return ptr.get() == child; });

    if (it != m_children.end()) {
        (*it)->m_parent = nullptr;
        m_children.erase(it);
        mark_layout_dirty();
    }
}

void UIElement::remove_all_children() {
    for (auto& child : m_children) {
        child->m_parent = nullptr;
    }
    m_children.clear();
    mark_layout_dirty();
}

UIElement* UIElement::find_child(const std::string& name) {
    for (auto& child : m_children) {
        if (child->get_name() == name) {
            return child.get();
        }
        if (auto* found = child->find_child(name)) {
            return found;
        }
    }
    return nullptr;
}

void UIElement::request_focus() {
    m_focused = true;
    on_focus_changed(true);
}

void UIElement::release_focus() {
    m_focused = false;
    on_focus_changed(false);
}

void UIElement::update(float dt, const UIInputState& input) {
    if (!m_visible) return;

    // Handle hover state
    bool was_hovered = m_hovered;
    m_hovered = m_interactive && hit_test(input.mouse_position);

    if (m_hovered != was_hovered && on_hover) {
        on_hover(m_hovered);
    }

    // Handle press state
    bool was_pressed = m_pressed;
    if (m_hovered && input.was_mouse_pressed(0)) {
        m_pressed = true;
    }
    if (m_pressed && input.was_mouse_released(0)) {
        m_pressed = false;
        if (m_hovered && m_interactive && m_enabled) {
            on_click_internal();
            if (on_click) {
                on_click();
            }
        }
    }

    // Custom update
    on_update(dt, input);

    // Update children
    for (auto& child : m_children) {
        child->update(dt, input);
    }
}

void UIElement::render(UIRenderContext& ctx) {
    if (!m_visible) return;

    // Render self
    on_render(ctx);

    // Render children
    for (auto& child : m_children) {
        child->render(ctx);
    }

    m_dirty = false;
}

void UIElement::on_render(UIRenderContext& ctx) {
    // Default: render background and border
    render_background(ctx, m_bounds);
}

void UIElement::render_background(UIRenderContext& ctx, const Rect& bounds) {
    StyleState state = get_current_state();
    const Vec4& bg_color = m_style.background_color.get(state);

    if (bg_color.a > 0.0f) {
        if (m_style.border_radius > 0.0f) {
            ctx.draw_rect_rounded(bounds, bg_color, m_style.border_radius);
        } else {
            ctx.draw_rect(bounds, bg_color);
        }
    }

    if (m_style.border_width > 0.0f) {
        const Vec4& border_color = m_style.border_color.get(state);
        if (border_color.a > 0.0f) {
            if (m_style.border_radius > 0.0f) {
                ctx.draw_rect_outline_rounded(bounds, border_color,
                                              m_style.border_width, m_style.border_radius);
            } else {
                ctx.draw_rect_outline(bounds, border_color, m_style.border_width);
            }
        }
    }
}

Vec2 UIElement::on_measure(Vec2 available_size) {
    // Default: use explicit size
    Vec2 size = m_size;

    // Apply size mode
    if (m_style.width_mode == SizeMode::FillParent) {
        size.x = available_size.x;
    } else if (m_style.width_mode == SizeMode::Percentage) {
        size.x = available_size.x * (m_style.width_percent / 100.0f);
    }

    if (m_style.height_mode == SizeMode::FillParent) {
        size.y = available_size.y;
    } else if (m_style.height_mode == SizeMode::Percentage) {
        size.y = available_size.y * (m_style.height_percent / 100.0f);
    }

    // Apply constraints
    if (m_style.min_width > 0.0f) size.x = std::max(size.x, m_style.min_width);
    if (m_style.min_height > 0.0f) size.y = std::max(size.y, m_style.min_height);
    if (m_style.max_width > 0.0f) size.x = std::min(size.x, m_style.max_width);
    if (m_style.max_height > 0.0f) size.y = std::min(size.y, m_style.max_height);

    return size;
}

void UIElement::layout(const Rect& parent_bounds) {
    // Measure
    Vec2 available = parent_bounds.size() - m_style.margin.total();
    Vec2 measured = on_measure(available);

    // Calculate position based on anchor
    Vec2 pos = m_position + Vec2(m_style.margin.left, m_style.margin.top);

    switch (m_anchor) {
        case Anchor::TopLeft:
            pos += Vec2(parent_bounds.x, parent_bounds.y);
            break;
        case Anchor::Top:
            pos += Vec2(parent_bounds.x + parent_bounds.width * 0.5f - measured.x * 0.5f,
                        parent_bounds.y);
            break;
        case Anchor::TopRight:
            pos += Vec2(parent_bounds.right() - measured.x,
                        parent_bounds.y);
            break;
        case Anchor::Left:
            pos += Vec2(parent_bounds.x,
                        parent_bounds.y + parent_bounds.height * 0.5f - measured.y * 0.5f);
            break;
        case Anchor::Center:
            pos += Vec2(parent_bounds.x + parent_bounds.width * 0.5f - measured.x * 0.5f,
                        parent_bounds.y + parent_bounds.height * 0.5f - measured.y * 0.5f);
            break;
        case Anchor::Right:
            pos += Vec2(parent_bounds.right() - measured.x,
                        parent_bounds.y + parent_bounds.height * 0.5f - measured.y * 0.5f);
            break;
        case Anchor::BottomLeft:
            pos += Vec2(parent_bounds.x,
                        parent_bounds.bottom() - measured.y);
            break;
        case Anchor::Bottom:
            pos += Vec2(parent_bounds.x + parent_bounds.width * 0.5f - measured.x * 0.5f,
                        parent_bounds.bottom() - measured.y);
            break;
        case Anchor::BottomRight:
            pos += Vec2(parent_bounds.right() - measured.x,
                        parent_bounds.bottom() - measured.y);
            break;
    }

    // Apply pivot
    pos -= m_pivot * measured;

    // Set bounds
    m_bounds = Rect(pos, measured);

    // Content bounds (inside padding)
    m_content_bounds = Rect(
        m_bounds.x + m_style.padding.left,
        m_bounds.y + m_style.padding.top,
        m_bounds.width - m_style.padding.horizontal(),
        m_bounds.height - m_style.padding.vertical()
    );

    // Custom layout
    on_layout(m_content_bounds);

    // Layout children
    for (auto& child : m_children) {
        child->layout(m_content_bounds);
    }

    m_layout_dirty = false;
}

Vec2 UIElement::measure(Vec2 available_size) {
    return on_measure(available_size);
}

void UIElement::mark_layout_dirty() {
    m_layout_dirty = true;
    if (m_parent) {
        m_parent->mark_layout_dirty();
    }
}

void UIElement::mark_dirty() {
    m_dirty = true;
}

bool UIElement::hit_test(Vec2 point) const {
    return m_bounds.contains(point);
}

UIElement* UIElement::find_element_at(Vec2 point) {
    if (!m_visible || !hit_test(point)) {
        return nullptr;
    }

    // Check children in reverse order (top to bottom)
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
        if (auto* found = (*it)->find_element_at(point)) {
            return found;
        }
    }

    return m_interactive ? this : nullptr;
}

StyleState UIElement::get_current_state() const {
    if (!m_enabled) return StyleState::Disabled;
    if (m_focused) return StyleState::Focused;
    if (m_pressed) return StyleState::Pressed;
    if (m_hovered) return StyleState::Hovered;
    return StyleState::Normal;
}

} // namespace engine::ui
