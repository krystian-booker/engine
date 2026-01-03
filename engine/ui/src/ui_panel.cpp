#include <engine/ui/ui_elements.hpp>
#include <engine/ui/ui_renderer.hpp>

namespace engine::ui {

UIPanel::UIPanel() {
    m_style = UIStyle::panel();
}

void UIPanel::render(UIRenderContext& ctx) {
    if (!is_visible()) return;

    // Render self (pushes clip rect if overflow mode)
    on_render(ctx);

    // Render children
    for (auto& child : m_children) {
        child->render(ctx);
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
    if (m_children.empty()) return;

    float offset = 0.0f;

    for (auto& child : m_children) {
        Vec2 child_size = child->measure(bounds.size());
        const EdgeInsets& margin = child->get_style().margin;

        Rect child_bounds;
        if (m_layout_direction == LayoutDirection::Horizontal) {
            child_bounds = Rect(
                bounds.x + offset + margin.left,
                bounds.y + margin.top,
                child_size.x,
                bounds.height - margin.vertical()
            );
            offset += child_size.x + margin.horizontal() + m_spacing;
        } else {
            child_bounds = Rect(
                bounds.x + margin.left,
                bounds.y + offset + margin.top,
                bounds.width - margin.horizontal(),
                child_size.y
            );
            offset += child_size.y + margin.vertical() + m_spacing;
        }

        child->layout(child_bounds);
    }
}

} // namespace engine::ui
