#include <engine/ui/ui_elements.hpp>
#include <engine/ui/ui_renderer.hpp>
#include <algorithm>

namespace engine::ui {

UISlider::UISlider() {
    m_style = UIStyle::slider();
    m_interactive = true;
}

void UISlider::set_value(float value) {
    float old_value = m_value;

    // Clamp to range
    value = std::clamp(value, m_min, m_max);

    // Apply step
    if (m_step > 0.0f) {
        value = m_min + std::round((value - m_min) / m_step) * m_step;
        value = std::clamp(value, m_min, m_max);
    }

    m_value = value;

    if (m_value != old_value && on_value_changed) {
        on_value_changed(m_value);
    }

    mark_dirty();
}

void UISlider::set_range(float min, float max) {
    m_min = min;
    m_max = max;
    set_value(m_value);  // Re-clamp current value
}

void UISlider::on_update(float /*dt*/, const UIInputState& input) {
    if (!m_enabled) return;

    // Start dragging
    if (is_hovered() && input.was_mouse_pressed(0)) {
        m_dragging = true;
    }

    // Stop dragging
    if (m_dragging && input.was_mouse_released(0)) {
        m_dragging = false;
    }

    // Update value while dragging
    if (m_dragging) {
        float pos;
        if (m_orientation == LayoutDirection::Horizontal) {
            pos = (input.mouse_position.x - m_content_bounds.x) / m_content_bounds.width;
        } else {
            pos = (input.mouse_position.y - m_content_bounds.y) / m_content_bounds.height;
        }
        pos = std::clamp(pos, 0.0f, 1.0f);
        set_value(m_min + pos * (m_max - m_min));
    }
}

void UISlider::on_render(UIRenderContext& ctx) {
    render_background(ctx, m_bounds);

    float normalized = (m_value - m_min) / (m_max - m_min);

    if (m_orientation == LayoutDirection::Horizontal) {
        // Track
        float track_height = 4.0f;
        float track_y = m_content_bounds.y + (m_content_bounds.height - track_height) * 0.5f;
        Rect track_rect(m_content_bounds.x, track_y, m_content_bounds.width, track_height);
        ctx.draw_rect_rounded(track_rect, m_track_color, track_height * 0.5f);

        // Fill
        Rect fill_rect(m_content_bounds.x, track_y,
                       m_content_bounds.width * normalized, track_height);
        ctx.draw_rect_rounded(fill_rect, m_fill_color, track_height * 0.5f);

        // Thumb
        float thumb_x = m_content_bounds.x + m_content_bounds.width * normalized - m_thumb_size * 0.5f;
        float thumb_y = m_content_bounds.y + (m_content_bounds.height - m_thumb_size) * 0.5f;
        Rect thumb_rect(thumb_x, thumb_y, m_thumb_size, m_thumb_size);

        Vec4 thumb_color = m_thumb_color;
        if (m_dragging || is_pressed()) {
            thumb_color *= 0.8f;
            thumb_color.a = 1.0f;
        } else if (is_hovered()) {
            thumb_color *= 1.1f;
            thumb_color.a = 1.0f;
        }
        ctx.draw_rect_rounded(thumb_rect, thumb_color, m_thumb_size * 0.5f);
    } else {
        // Vertical slider
        float track_width = 4.0f;
        float track_x = m_content_bounds.x + (m_content_bounds.width - track_width) * 0.5f;
        Rect track_rect(track_x, m_content_bounds.y, track_width, m_content_bounds.height);
        ctx.draw_rect_rounded(track_rect, m_track_color, track_width * 0.5f);

        // Fill (from bottom)
        float fill_height = m_content_bounds.height * normalized;
        Rect fill_rect(track_x, m_content_bounds.bottom() - fill_height,
                       track_width, fill_height);
        ctx.draw_rect_rounded(fill_rect, m_fill_color, track_width * 0.5f);

        // Thumb
        float thumb_x = m_content_bounds.x + (m_content_bounds.width - m_thumb_size) * 0.5f;
        float thumb_y = m_content_bounds.bottom() - m_content_bounds.height * normalized - m_thumb_size * 0.5f;
        Rect thumb_rect(thumb_x, thumb_y, m_thumb_size, m_thumb_size);

        Vec4 thumb_color = m_thumb_color;
        if (m_dragging || is_pressed()) {
            thumb_color *= 0.8f;
            thumb_color.a = 1.0f;
        } else if (is_hovered()) {
            thumb_color *= 1.1f;
            thumb_color.a = 1.0f;
        }
        ctx.draw_rect_rounded(thumb_rect, thumb_color, m_thumb_size * 0.5f);
    }
}

Vec2 UISlider::on_measure(Vec2 available_size) {
    Vec2 size = UIElement::on_measure(available_size);

    // Ensure minimum size for thumb
    if (m_orientation == LayoutDirection::Horizontal) {
        size.y = std::max(size.y, m_thumb_size);
    } else {
        size.x = std::max(size.x, m_thumb_size);
    }

    return size;
}

float UISlider::value_from_position(float pos) {
    return m_min + pos * (m_max - m_min);
}

float UISlider::position_from_value(float value) {
    return (value - m_min) / (m_max - m_min);
}

// Progress bar implementation

UIProgressBar::UIProgressBar() {
    m_style.min_height = 8.0f;
}

void UIProgressBar::set_value(float value) {
    m_value = std::clamp(value, 0.0f, 1.0f);
    mark_dirty();
}

void UIProgressBar::on_render(UIRenderContext& ctx) {
    // Track
    ctx.draw_rect_rounded(m_bounds, m_track_color, m_style.border_radius);

    // Fill
    if (m_value > 0.0f) {
        Rect fill_rect;
        if (m_orientation == LayoutDirection::Horizontal) {
            fill_rect = Rect(m_bounds.x, m_bounds.y,
                             m_bounds.width * m_value, m_bounds.height);
        } else {
            float fill_height = m_bounds.height * m_value;
            fill_rect = Rect(m_bounds.x, m_bounds.bottom() - fill_height,
                             m_bounds.width, fill_height);
        }
        ctx.draw_rect_rounded(fill_rect, m_fill_color, m_style.border_radius);
    }
}

Vec2 UIProgressBar::on_measure(Vec2 available_size) {
    return UIElement::on_measure(available_size);
}

// Toggle implementation

UIToggle::UIToggle() {
    m_style = UIStyle::button();
    m_interactive = true;
}

UIToggle::UIToggle(const std::string& label) : UIToggle() {
    m_label = label;
}

void UIToggle::set_checked(bool checked) {
    if (m_checked != checked) {
        m_checked = checked;
        if (on_toggled) {
            on_toggled(m_checked);
        }
        mark_dirty();
    }
}

void UIToggle::on_render(UIRenderContext& ctx) {
    StyleState state = get_current_state();

    // Checkbox box
    float box_y = m_bounds.y + (m_bounds.height - m_box_size) * 0.5f;
    Rect box_rect(m_bounds.x, box_y, m_box_size, m_box_size);

    // Box background
    Vec4 box_color = m_checked ?
        Vec4(0.3f, 0.5f, 0.9f, 1.0f) :
        m_style.background_color.get(state);
    ctx.draw_rect_rounded(box_rect, box_color, 3.0f);

    // Check mark
    if (m_checked) {
        // Simple check mark (could be improved with actual path rendering)
        Vec4 check_color(1.0f, 1.0f, 1.0f, 1.0f);
        float inset = m_box_size * 0.25f;
        Rect inner(box_rect.x + inset, box_rect.y + inset,
                   m_box_size - inset * 2, m_box_size - inset * 2);
        ctx.draw_rect_rounded(inner, check_color, 2.0f);
    }

    // Label
    if (!m_label.empty()) {
        const Vec4& text_color = m_style.text_color.get(state);
        Vec2 text_pos(m_bounds.x + m_box_size + 8.0f,
                      m_bounds.y + m_bounds.height * 0.5f);
        ctx.draw_text(m_label, text_pos, m_style.font,
                      m_style.font_size, text_color, HAlign::Left);
    }
}

Vec2 UIToggle::on_measure(Vec2 available_size) {
    Vec2 size = UIElement::on_measure(available_size);

    float width = m_box_size;
    if (!m_label.empty()) {
        width += 8.0f + m_label.length() * m_style.font_size * 0.6f;
    }
    float height = std::max(m_box_size, m_style.font_size * 1.2f);

    if (m_style.width_mode == SizeMode::FitContent) {
        size.x = width + m_style.padding.horizontal();
    }
    if (m_style.height_mode == SizeMode::FitContent) {
        size.y = height + m_style.padding.vertical();
    }

    return size;
}

void UIToggle::on_click_internal() {
    set_checked(!m_checked);
}

} // namespace engine::ui
