#include <engine/ui/ui_elements.hpp>
#include <engine/ui/ui_renderer.hpp>
#include <algorithm>

namespace engine::ui {

UIPanel::UIPanel() {
    m_style = UIStyle::panel();
}

void UIPanel::set_scroll_offset(Vec2 offset) {
    Vec2 max_scroll = get_max_scroll();
    offset.x = std::clamp(offset.x, 0.0f, max_scroll.x);
    offset.y = std::clamp(offset.y, 0.0f, max_scroll.y);
    if (m_scroll_offset.x != offset.x || m_scroll_offset.y != offset.y) {
        m_scroll_offset = offset;
        mark_layout_dirty();
    }
}

Vec2 UIPanel::get_max_scroll() const {
    Vec2 max_scroll{0.0f};
    max_scroll.x = std::max(0.0f, m_content_size.x - m_content_bounds.width);
    max_scroll.y = std::max(0.0f, m_content_size.y - m_content_bounds.height);
    return max_scroll;
}

void UIPanel::scroll_to_bottom() {
    Vec2 max_scroll = get_max_scroll();
    set_scroll_offset({m_scroll_offset.x, max_scroll.y});
}

void UIPanel::on_update(float dt, const UIInputState& input) {
    if (m_overflow != Overflow::Scroll) return;
    if (!is_hovered()) return;

    // Handle mouse wheel scrolling
    if (input.scroll_delta.y != 0.0f) {
        float scroll_speed = 30.0f;
        Vec2 new_offset = m_scroll_offset;
        new_offset.y -= input.scroll_delta.y * scroll_speed;
        set_scroll_offset(new_offset);
    }
    if (input.scroll_delta.x != 0.0f) {
        float scroll_speed = 30.0f;
        Vec2 new_offset = m_scroll_offset;
        new_offset.x -= input.scroll_delta.x * scroll_speed;
        set_scroll_offset(new_offset);
    }
}

void UIPanel::render(UIRenderContext& ctx) {
    if (!is_visible()) return;

    // Render self (pushes clip rect if overflow mode)
    on_render(ctx);

    // Render children
    for (auto& child : m_children) {
        child->render(ctx);
    }

    // Render scrollbar if needed
    if (m_overflow == Overflow::Scroll && m_show_scrollbar) {
        render_scrollbar(ctx);
    }

    // Pop clip rect if we pushed one
    if (m_overflow == Overflow::Hidden || m_overflow == Overflow::Scroll) {
        ctx.pop_clip_rect();
    }

    m_dirty = false;
}

void UIPanel::on_render(UIRenderContext& ctx) {
    render_background(ctx, m_bounds);

    // Handle scrolling/clipping - push clip rect, will be popped in render()
    if (m_overflow == Overflow::Hidden || m_overflow == Overflow::Scroll) {
        ctx.push_clip_rect(m_content_bounds);
    }
}

void UIPanel::render_scrollbar(UIRenderContext& ctx) {
    Vec2 max_scroll = get_max_scroll();

    // Vertical scrollbar
    if (max_scroll.y > 0.0f) {
        float track_height = m_content_bounds.height;
        float visible_ratio = m_content_bounds.height / m_content_size.y;
        float thumb_height = std::max(20.0f, track_height * visible_ratio);
        float scroll_ratio = m_scroll_offset.y / max_scroll.y;
        float thumb_y = m_content_bounds.y + scroll_ratio * (track_height - thumb_height);

        // Track
        Rect track_rect(
            m_content_bounds.right() - m_scrollbar_width,
            m_content_bounds.y,
            m_scrollbar_width,
            track_height
        );
        ctx.draw_rect(track_rect, Vec4(0.1f, 0.1f, 0.1f, 0.5f));

        // Thumb
        Rect thumb_rect(
            m_content_bounds.right() - m_scrollbar_width,
            thumb_y,
            m_scrollbar_width,
            thumb_height
        );
        ctx.draw_rect_rounded(thumb_rect, Vec4(0.5f, 0.5f, 0.5f, 0.8f), m_scrollbar_width * 0.5f);
    }

    // Horizontal scrollbar
    if (max_scroll.x > 0.0f) {
        float track_width = m_content_bounds.width;
        float visible_ratio = m_content_bounds.width / m_content_size.x;
        float thumb_width = std::max(20.0f, track_width * visible_ratio);
        float scroll_ratio = m_scroll_offset.x / max_scroll.x;
        float thumb_x = m_content_bounds.x + scroll_ratio * (track_width - thumb_width);

        // Track
        Rect track_rect(
            m_content_bounds.x,
            m_content_bounds.bottom() - m_scrollbar_width,
            track_width,
            m_scrollbar_width
        );
        ctx.draw_rect(track_rect, Vec4(0.1f, 0.1f, 0.1f, 0.5f));

        // Thumb
        Rect thumb_rect(
            thumb_x,
            m_content_bounds.bottom() - m_scrollbar_width,
            thumb_width,
            m_scrollbar_width
        );
        ctx.draw_rect_rounded(thumb_rect, Vec4(0.5f, 0.5f, 0.5f, 0.8f), m_scrollbar_width * 0.5f);
    }
}

Vec2 UIPanel::calculate_content_size() {
    Vec2 content_size{0.0f};

    for (auto& child : m_children) {
        Vec2 child_size = child->measure(Vec2(10000.0f, 10000.0f));
        child_size += child->get_style().margin.total();

        if (m_layout_direction == LayoutDirection::Horizontal) {
            content_size.x += child_size.x;
            content_size.y = std::max(content_size.y, child_size.y);
        } else {
            content_size.x = std::max(content_size.x, child_size.x);
            content_size.y += child_size.y;
        }
    }

    // Add spacing between children
    if (!m_children.empty()) {
        if (m_layout_direction == LayoutDirection::Horizontal) {
            content_size.x += m_spacing * (m_children.size() - 1);
        } else {
            content_size.y += m_spacing * (m_children.size() - 1);
        }
    }

    return content_size;
}

Vec2 UIPanel::on_measure(Vec2 available_size) {
    Vec2 size = UIElement::on_measure(available_size);

    // If FitContent, measure children
    if (m_style.width_mode == SizeMode::FitContent ||
        m_style.height_mode == SizeMode::FitContent) {

        Vec2 content_size{0.0f};

        for (auto& child : m_children) {
            Vec2 child_size = child->measure(available_size);
            child_size += child->get_style().margin.total();

            if (m_layout_direction == LayoutDirection::Horizontal) {
                content_size.x += child_size.x;
                content_size.y = std::max(content_size.y, child_size.y);
            } else {
                content_size.x = std::max(content_size.x, child_size.x);
                content_size.y += child_size.y;
            }
        }

        // Add spacing between children
        if (!m_children.empty()) {
            if (m_layout_direction == LayoutDirection::Horizontal) {
                content_size.x += m_spacing * (m_children.size() - 1);
            } else {
                content_size.y += m_spacing * (m_children.size() - 1);
            }
        }

        // Add padding
        content_size += m_style.padding.total();

        if (m_style.width_mode == SizeMode::FitContent) {
            size.x = content_size.x;
        }
        if (m_style.height_mode == SizeMode::FitContent) {
            size.y = content_size.y;
        }
    }

    return size;
}

void UIPanel::on_layout(const Rect& bounds) {
    // Calculate content size for scrolling
    m_content_size = calculate_content_size();

    if (m_children.empty()) return;

    float offset = 0.0f;

    // Apply scroll offset
    float scroll_x = (m_overflow == Overflow::Scroll) ? -m_scroll_offset.x : 0.0f;
    float scroll_y = (m_overflow == Overflow::Scroll) ? -m_scroll_offset.y : 0.0f;

    for (auto& child : m_children) {
        Vec2 child_size = child->measure(bounds.size());
        const EdgeInsets& margin = child->get_style().margin;

        Rect child_bounds;
        if (m_layout_direction == LayoutDirection::Horizontal) {
            child_bounds = Rect(
                bounds.x + offset + margin.left + scroll_x,
                bounds.y + margin.top + scroll_y,
                child_size.x,
                bounds.height - margin.vertical()
            );
            offset += child_size.x + margin.horizontal() + m_spacing;
        } else {
            child_bounds = Rect(
                bounds.x + margin.left + scroll_x,
                bounds.y + offset + margin.top + scroll_y,
                bounds.width - margin.horizontal(),
                child_size.y
            );
            offset += child_size.y + margin.vertical() + m_spacing;
        }

        child->layout(child_bounds);
    }
}

} // namespace engine::ui
