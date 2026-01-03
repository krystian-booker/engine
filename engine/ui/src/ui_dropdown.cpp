#include <engine/ui/ui_elements.hpp>
#include <engine/ui/ui_renderer.hpp>
#include <engine/ui/ui_context.hpp>
#include <algorithm>

namespace engine::ui {

UIDropdown::UIDropdown() {
    m_style = UIStyle::button();
    m_interactive = true;
}

void UIDropdown::add_item(const std::string& id, const std::string& label) {
    m_items.push_back({id, label});
    mark_dirty();
}

void UIDropdown::clear_items() {
    m_items.clear();
    m_selected_id.clear();
    m_dropdown_scroll = 0.0f;
    mark_dirty();
}

void UIDropdown::set_selected_id(const std::string& id) {
    if (m_selected_id != id) {
        m_selected_id = id;
        mark_dirty();
    }
}

const std::string& UIDropdown::get_selected_label() const {
    static std::string empty;
    for (const auto& item : m_items) {
        if (item.id == m_selected_id) {
            return item.label;
        }
    }
    return empty;
}

void UIDropdown::open() {
    if (!m_is_open && !m_items.empty()) {
        m_is_open = true;
        m_hovered_item = -1;
        m_dropdown_scroll = 0.0f;

        // Calculate dropdown bounds (below the button)
        int visible_items = std::min(m_max_visible_items, static_cast<int>(m_items.size()));
        float dropdown_height = visible_items * m_item_height;

        m_dropdown_bounds = Rect(
            m_bounds.x,
            m_bounds.bottom(),
            m_bounds.width,
            dropdown_height
        );

        mark_dirty();
    }
}

void UIDropdown::close() {
    if (m_is_open) {
        m_is_open = false;
        m_hovered_item = -1;
        mark_dirty();
    }
}

void UIDropdown::on_click_internal() {
    toggle();
}

void UIDropdown::on_update(float dt, const UIInputState& input) {
    if (!m_is_open) return;

    // Check if mouse is in dropdown area
    bool in_dropdown = m_dropdown_bounds.contains(input.mouse_position);
    bool in_button = m_bounds.contains(input.mouse_position);

    if (in_dropdown) {
        // Update hovered item
        m_hovered_item = get_item_at_position(input.mouse_position);

        // Handle scroll wheel
        if (input.scroll_delta.y != 0.0f) {
            float max_scroll = std::max(0.0f,
                static_cast<float>(m_items.size()) * m_item_height - m_dropdown_bounds.height);
            m_dropdown_scroll = std::clamp(
                m_dropdown_scroll - input.scroll_delta.y * 20.0f,
                0.0f, max_scroll
            );
            mark_dirty();
        }

        // Handle click on item
        if (input.was_mouse_released(0) && m_hovered_item >= 0) {
            const auto& item = m_items[m_hovered_item];
            if (m_selected_id != item.id) {
                m_selected_id = item.id;
                if (on_selection_changed) {
                    on_selection_changed(item.id, item.label);
                }
            }
            close();
        }
    } else {
        m_hovered_item = -1;

        // Click outside closes dropdown (but not on the button itself)
        if (input.was_mouse_pressed(0) && !in_button) {
            close();
        }
    }
}

void UIDropdown::render(UIRenderContext& ctx) {
    if (!is_visible()) return;

    // Render the button part
    on_render(ctx);

    // Render dropdown list if open (without clipping so it renders over other elements)
    if (m_is_open) {
        render_dropdown_list(ctx);
    }

    m_dirty = false;
}

void UIDropdown::on_render(UIRenderContext& ctx) {
    // Render button background
    render_background(ctx, m_bounds);

    StyleState state = get_current_state();
    const Vec4& text_color = m_style.text_color.get(state);

    // Get display text
    const std::string& label = get_selected_label();
    const std::string& display_text = label.empty() ? m_placeholder : label;

    // Draw text (left-aligned with padding)
    Vec2 text_pos(m_content_bounds.x, m_content_bounds.center().y);
    ctx.draw_text(display_text, text_pos, m_style.font,
                  m_style.font_size, label.empty() ? Vec4(text_color.r, text_color.g, text_color.b, text_color.a * 0.5f) : text_color,
                  HAlign::Left);

    // Draw dropdown arrow
    float arrow_size = 8.0f;
    float arrow_x = m_content_bounds.right() - arrow_size - 4.0f;
    float arrow_y = m_content_bounds.center().y;

    // Simple triangle pointing down (or up if open)
    Vec4 arrow_color = text_color;
    if (m_is_open) {
        // Up arrow
        ctx.draw_rect(Rect(arrow_x, arrow_y - 2.0f, arrow_size, 2.0f), arrow_color);
    } else {
        // Down arrow
        ctx.draw_rect(Rect(arrow_x, arrow_y, arrow_size, 2.0f), arrow_color);
    }
}

void UIDropdown::render_dropdown_list(UIRenderContext& ctx) {
    // Background
    ctx.draw_rect(m_dropdown_bounds, Vec4(0.15f, 0.15f, 0.15f, 0.98f));

    // Border
    ctx.draw_rect_outline(m_dropdown_bounds, Vec4(0.3f, 0.3f, 0.3f, 1.0f), 1.0f);

    // Clip content
    ctx.push_clip_rect(m_dropdown_bounds);

    // Draw items
    float y = m_dropdown_bounds.y - m_dropdown_scroll;
    for (size_t i = 0; i < m_items.size(); ++i) {
        Rect item_rect(m_dropdown_bounds.x, y, m_dropdown_bounds.width, m_item_height);

        // Only render visible items
        if (item_rect.bottom() > m_dropdown_bounds.y && item_rect.y < m_dropdown_bounds.bottom()) {
            // Highlight hovered or selected item
            bool is_selected = (m_items[i].id == m_selected_id);
            bool is_hovered = (static_cast<int>(i) == m_hovered_item);

            if (is_hovered) {
                ctx.draw_rect(item_rect, Vec4(0.3f, 0.5f, 0.9f, 0.8f));
            } else if (is_selected) {
                ctx.draw_rect(item_rect, Vec4(0.25f, 0.25f, 0.25f, 1.0f));
            }

            // Draw label
            Vec2 text_pos(item_rect.x + 8.0f, item_rect.center().y);
            Vec4 text_color = is_hovered ? Vec4(1.0f) : Vec4(0.9f, 0.9f, 0.9f, 1.0f);
            ctx.draw_text(m_items[i].label, text_pos, m_style.font,
                          m_style.font_size, text_color, HAlign::Left);
        }

        y += m_item_height;
    }

    ctx.pop_clip_rect();

    // Draw scrollbar if needed
    float content_height = static_cast<float>(m_items.size()) * m_item_height;
    if (content_height > m_dropdown_bounds.height) {
        float scrollbar_width = 6.0f;
        float visible_ratio = m_dropdown_bounds.height / content_height;
        float thumb_height = std::max(20.0f, m_dropdown_bounds.height * visible_ratio);
        float max_scroll = content_height - m_dropdown_bounds.height;
        float scroll_ratio = m_dropdown_scroll / max_scroll;
        float thumb_y = m_dropdown_bounds.y + scroll_ratio * (m_dropdown_bounds.height - thumb_height);

        // Track
        Rect track_rect(
            m_dropdown_bounds.right() - scrollbar_width - 2.0f,
            m_dropdown_bounds.y,
            scrollbar_width,
            m_dropdown_bounds.height
        );
        ctx.draw_rect(track_rect, Vec4(0.1f, 0.1f, 0.1f, 0.5f));

        // Thumb
        Rect thumb_rect(
            m_dropdown_bounds.right() - scrollbar_width - 2.0f,
            thumb_y,
            scrollbar_width,
            thumb_height
        );
        ctx.draw_rect_rounded(thumb_rect, Vec4(0.5f, 0.5f, 0.5f, 0.8f), 3.0f);
    }
}

Vec2 UIDropdown::on_measure(Vec2 available_size) {
    Vec2 size = UIElement::on_measure(available_size);

    // Ensure minimum width
    size.x = std::max(size.x, 120.0f);
    size.y = std::max(size.y, 32.0f);

    return size;
}

int UIDropdown::get_item_at_position(Vec2 pos) const {
    if (!m_dropdown_bounds.contains(pos)) {
        return -1;
    }

    float relative_y = pos.y - m_dropdown_bounds.y + m_dropdown_scroll;
    int index = static_cast<int>(relative_y / m_item_height);

    if (index >= 0 && index < static_cast<int>(m_items.size())) {
        return index;
    }

    return -1;
}

} // namespace engine::ui
