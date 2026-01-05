#include <engine/ui/ui_tab_control.hpp>
#include <engine/ui/ui_renderer.hpp>
#include <engine/ui/ui_context.hpp>
#include <engine/localization/localization.hpp>
#include <algorithm>
#include <cmath>

namespace engine::ui {

UITabControl::UITabControl() {
    set_focusable(true);
    m_style = UIStyle::panel();
}

void UITabControl::add_tab(const std::string& id, const std::string& label) {
    TabItem tab;
    tab.id = id;
    tab.label = label;
    m_tabs.push_back(tab);
    mark_layout_dirty();

    // Select first tab by default
    if (m_selected_id.empty() && !m_tabs.empty()) {
        m_selected_id = m_tabs[0].id;
        update_content_visibility();
    }
}

void UITabControl::add_tab_localized(const std::string& id, const std::string& label_key) {
    TabItem tab;
    tab.id = id;
    tab.label_key = label_key;
    m_tabs.push_back(tab);
    mark_layout_dirty();

    // Select first tab by default
    if (m_selected_id.empty() && !m_tabs.empty()) {
        m_selected_id = m_tabs[0].id;
        update_content_visibility();
    }
}

void UITabControl::remove_tab(const std::string& id) {
    auto it = std::find_if(m_tabs.begin(), m_tabs.end(),
                           [&id](const TabItem& tab) { return tab.id == id; });
    if (it != m_tabs.end()) {
        int index = static_cast<int>(std::distance(m_tabs.begin(), it));
        m_tabs.erase(it);
        mark_layout_dirty();

        // If removed tab was selected, select first available
        if (m_selected_id == id) {
            m_selected_id = m_tabs.empty() ? "" : m_tabs[0].id;
            update_content_visibility();
            if (on_tab_changed && !m_selected_id.empty()) {
                on_tab_changed(m_selected_id, 0);
            }
        }
    }
}

void UITabControl::clear_tabs() {
    m_tabs.clear();
    m_tab_bounds.clear();
    m_selected_id.clear();
    mark_layout_dirty();
}

void UITabControl::set_selected_tab(const std::string& id) {
    auto it = std::find_if(m_tabs.begin(), m_tabs.end(),
                           [&id](const TabItem& tab) { return tab.id == id; });

    if (it != m_tabs.end()) {
        if (m_selected_id != id) {
            m_selected_id = id;
            int index = static_cast<int>(std::distance(m_tabs.begin(), it));
            update_content_visibility();
            mark_dirty();

            if (on_tab_changed) {
                on_tab_changed(id, index);
            }
        }
    }
}

void UITabControl::set_selected_index(int index) {
    if (index >= 0 && index < static_cast<int>(m_tabs.size())) {
        set_selected_tab(m_tabs[index].id);
    }
}

int UITabControl::get_selected_index() const {
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].id == m_selected_id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

UIElement* UITabControl::get_tab_content(const std::string& id) {
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].id == id) {
            return get_tab_content(static_cast<int>(i));
        }
    }
    return nullptr;
}

UIElement* UITabControl::get_tab_content(int index) {
    const auto& children = get_children();
    if (index >= 0 && index < static_cast<int>(children.size())) {
        return children[index].get();
    }
    return nullptr;
}

UIElement* UITabControl::get_active_content() {
    int index = get_selected_index();
    return get_tab_content(index);
}

std::string UITabControl::get_resolved_label(const TabItem& tab) const {
    if (!tab.label_key.empty()) {
        return engine::localization::loc(tab.label_key);
    }
    return tab.label;
}

bool UITabControl::is_horizontal() const {
    return m_tab_position == TabPosition::Top || m_tab_position == TabPosition::Bottom;
}

float UITabControl::get_tab_bar_size() const {
    return m_tab_height;
}

int UITabControl::get_tab_at_position(Vec2 pos) const {
    for (size_t i = 0; i < m_tab_bounds.size(); ++i) {
        if (m_tab_bounds[i].contains(pos)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void UITabControl::update_content_visibility() {
    const auto& children = get_children();
    int selected_index = get_selected_index();

    for (size_t i = 0; i < children.size(); ++i) {
        children[i]->set_visible(static_cast<int>(i) == selected_index);
    }
}

void UITabControl::compute_tab_bounds() {
    m_tab_bounds.resize(m_tabs.size());

    UIContext* ctx = get_ui_context();
    FontHandle font = ctx ? ctx->get_default_font() : FontHandle{};

    if (is_horizontal()) {
        // Horizontal tab bar (top or bottom)
        float x = m_tab_bar_bounds.x;
        float y = m_tab_bar_bounds.y;
        float bar_height = m_tab_bar_bounds.height;

        for (size_t i = 0; i < m_tabs.size(); ++i) {
            // Measure tab width based on text
            std::string label = get_resolved_label(m_tabs[i]);
            float text_width = 80.0f; // Default width

            if (ctx) {
                Vec2 text_size = ctx->font_manager().measure_text(font, label);
                text_width = text_size.x;
            }

            float tab_width = text_width + m_tab_padding * 2.0f;
            m_tab_bounds[i] = Rect(x, y, tab_width, bar_height);
            x += tab_width + m_tab_spacing;
        }
    } else {
        // Vertical tab bar (left or right)
        float x = m_tab_bar_bounds.x;
        float y = m_tab_bar_bounds.y;
        float bar_width = m_tab_bar_bounds.width;

        for (size_t i = 0; i < m_tabs.size(); ++i) {
            m_tab_bounds[i] = Rect(x, y, bar_width, m_tab_height);
            y += m_tab_height + m_tab_spacing;
        }
    }
}

void UITabControl::on_update(float dt, const UIInputState& input) {
    if (!is_enabled()) {
        m_hovered_tab = -1;
        m_pressed_tab = -1;
        return;
    }

    // Update hover state
    int prev_hovered = m_hovered_tab;
    m_hovered_tab = get_tab_at_position(input.mouse_position);

    // Handle click
    if (input.was_mouse_pressed(0) && m_hovered_tab >= 0) {
        m_pressed_tab = m_hovered_tab;
    }

    if (m_pressed_tab >= 0 && !input.mouse_buttons[0]) {
        // Mouse released - check if still over the same tab
        if (m_hovered_tab == m_pressed_tab) {
            set_selected_index(m_pressed_tab);
        }
        m_pressed_tab = -1;
    }

    // Handle keyboard navigation when focused
    if (is_focused()) {
        int current_index = get_selected_index();

        bool prev_key = is_horizontal()
            ? input.was_nav_pressed(NavDirection::Left)
            : input.was_nav_pressed(NavDirection::Up);
        bool next_key = is_horizontal()
            ? input.was_nav_pressed(NavDirection::Right)
            : input.was_nav_pressed(NavDirection::Down);

        if (prev_key && current_index > 0) {
            set_selected_index(current_index - 1);
        } else if (next_key && current_index < static_cast<int>(m_tabs.size()) - 1) {
            set_selected_index(current_index + 1);
        }
    }
}

void UITabControl::on_render(UIRenderContext& ctx) {
    // Render panel background for content area
    render_background(ctx, m_content_bounds);

    // Render tab bar background
    ctx.draw_rect(m_tab_bar_bounds, Vec4(0.1f, 0.1f, 0.1f, 1.0f));

    // Render each tab
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        bool selected = (m_tabs[i].id == m_selected_id);
        bool hovered = (static_cast<int>(i) == m_hovered_tab);
        bool pressed = (static_cast<int>(i) == m_pressed_tab);

        render_tab(ctx, m_tabs[i], m_tab_bounds[i], selected, hovered || pressed);
    }
}

void UITabControl::render_tab(UIRenderContext& ctx, const TabItem& tab, const Rect& bounds,
                               bool selected, bool hovered) {
    // Determine tab background color
    Vec4 bg_color = m_tab_color;
    if (selected) {
        bg_color = m_tab_selected_color;
    } else if (hovered) {
        bg_color = m_tab_hover_color;
    }

    // Draw tab background with rounded top corners
    float corner_radius = 4.0f;
    ctx.draw_rect_rounded(bounds, bg_color, corner_radius);

    // Draw selected indicator line
    if (selected) {
        float line_height = 3.0f;
        Rect indicator;

        switch (m_tab_position) {
            case TabPosition::Top:
                indicator = Rect(bounds.x, bounds.y + bounds.height - line_height,
                                 bounds.width, line_height);
                break;
            case TabPosition::Bottom:
                indicator = Rect(bounds.x, bounds.y, bounds.width, line_height);
                break;
            case TabPosition::Left:
                indicator = Rect(bounds.x + bounds.width - line_height, bounds.y,
                                 line_height, bounds.height);
                break;
            case TabPosition::Right:
                indicator = Rect(bounds.x, bounds.y, line_height, bounds.height);
                break;
        }

        ctx.draw_rect(indicator, Vec4(0.4f, 0.6f, 1.0f, 1.0f));
    }

    // Draw tab label
    std::string label = get_resolved_label(tab);
    if (!label.empty()) {
        FontHandle font = ctx.get_font_manager()->get_default_font();
        float font_size = 14.0f;

        Vec2 text_pos = bounds.center();
        ctx.draw_text(label, text_pos, font, font_size, m_tab_text_color, HAlign::Center);
    }
}

Vec2 UITabControl::on_measure(Vec2 available_size) {
    Vec2 size = UIElement::on_measure(available_size);

    // Add tab bar size to base measurement
    float bar_size = get_tab_bar_size();

    if (is_horizontal()) {
        size.y += bar_size;
    } else {
        // For vertical tabs, estimate bar width
        float bar_width = 100.0f; // Default
        UIContext* ctx = get_ui_context();
        if (ctx) {
            FontHandle font = ctx->get_default_font();
            for (const auto& tab : m_tabs) {
                std::string label = get_resolved_label(tab);
                Vec2 text_size = ctx->font_manager().measure_text(font, label);
                bar_width = std::max(bar_width, text_size.x + m_tab_padding * 2.0f);
            }
        }
        size.x += bar_width;
    }

    return size;
}

void UITabControl::on_layout(const Rect& bounds) {
    float bar_size = get_tab_bar_size();

    // Compute tab bar and content bounds based on position
    switch (m_tab_position) {
        case TabPosition::Top:
            m_tab_bar_bounds = Rect(bounds.x, bounds.y, bounds.width, bar_size);
            m_content_bounds = Rect(bounds.x, bounds.y + bar_size,
                                    bounds.width, bounds.height - bar_size);
            break;

        case TabPosition::Bottom:
            m_content_bounds = Rect(bounds.x, bounds.y,
                                    bounds.width, bounds.height - bar_size);
            m_tab_bar_bounds = Rect(bounds.x, bounds.y + bounds.height - bar_size,
                                    bounds.width, bar_size);
            break;

        case TabPosition::Left: {
            // Calculate bar width from tab labels
            float bar_width = 100.0f;
            UIContext* ctx = get_ui_context();
            if (ctx) {
                FontHandle font = ctx->get_default_font();
                for (const auto& tab : m_tabs) {
                    std::string label = get_resolved_label(tab);
                    Vec2 text_size = ctx->font_manager().measure_text(font, label);
                    bar_width = std::max(bar_width, text_size.x + m_tab_padding * 2.0f);
                }
            }
            m_tab_bar_bounds = Rect(bounds.x, bounds.y, bar_width, bounds.height);
            m_content_bounds = Rect(bounds.x + bar_width, bounds.y,
                                    bounds.width - bar_width, bounds.height);
            break;
        }

        case TabPosition::Right: {
            // Calculate bar width from tab labels
            float bar_width = 100.0f;
            UIContext* ctx = get_ui_context();
            if (ctx) {
                FontHandle font = ctx->get_default_font();
                for (const auto& tab : m_tabs) {
                    std::string label = get_resolved_label(tab);
                    Vec2 text_size = ctx->font_manager().measure_text(font, label);
                    bar_width = std::max(bar_width, text_size.x + m_tab_padding * 2.0f);
                }
            }
            m_content_bounds = Rect(bounds.x, bounds.y,
                                    bounds.width - bar_width, bounds.height);
            m_tab_bar_bounds = Rect(bounds.x + bounds.width - bar_width, bounds.y,
                                    bar_width, bounds.height);
            break;
        }
    }

    compute_tab_bounds();

    // Layout children (content panels) within content bounds
    const auto& children = get_children();
    for (auto& child : children) {
        child->layout(m_content_bounds);
    }

    // Update content visibility
    update_content_visibility();
}

} // namespace engine::ui
