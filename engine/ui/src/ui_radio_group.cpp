#include <engine/ui/ui_radio_group.hpp>
#include <engine/ui/ui_renderer.hpp>
#include <engine/localization/localization.hpp>
#include <algorithm>
#include <cmath>

namespace engine::ui {

UIRadioGroup::UIRadioGroup() {
    set_focusable(true);
}

void UIRadioGroup::add_option(const std::string& id, const std::string& label) {
    RadioOption option;
    option.id = id;
    option.label = label;
    add_option(option);
}

void UIRadioGroup::add_option(const RadioOption& option) {
    m_options.push_back(option);
    mark_layout_dirty();

    // Select first option by default if nothing selected
    if (m_selected_id.empty() && !m_options.empty()) {
        m_selected_id = m_options[0].id;
    }
}

void UIRadioGroup::add_options(const std::vector<RadioOption>& options) {
    for (const auto& opt : options) {
        add_option(opt);
    }
}

void UIRadioGroup::remove_option(const std::string& id) {
    auto it = std::find_if(m_options.begin(), m_options.end(),
                           [&id](const RadioOption& opt) { return opt.id == id; });
    if (it != m_options.end()) {
        m_options.erase(it);
        mark_layout_dirty();

        // If removed option was selected, select first available
        if (m_selected_id == id) {
            m_selected_id = m_options.empty() ? "" : m_options[0].id;
            if (on_selection_changed) {
                on_selection_changed(m_selected_id);
            }
        }
    }
}

void UIRadioGroup::clear_options() {
    m_options.clear();
    m_option_layouts.clear();
    m_selected_id.clear();
    mark_layout_dirty();
}

void UIRadioGroup::set_selected(const std::string& id) {
    // Find option
    auto it = std::find_if(m_options.begin(), m_options.end(),
                           [&id](const RadioOption& opt) { return opt.id == id; });

    if (it != m_options.end() && it->enabled) {
        if (m_selected_id != id) {
            m_selected_id = id;
            mark_dirty();

            if (on_selection_changed) {
                on_selection_changed(id);
            }
        }
    }
}

int UIRadioGroup::get_selected_index() const {
    for (size_t i = 0; i < m_options.size(); ++i) {
        if (m_options[i].id == m_selected_id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void UIRadioGroup::select_index(int index) {
    if (index >= 0 && index < static_cast<int>(m_options.size())) {
        set_selected(m_options[index].id);
    }
}

void UIRadioGroup::set_option_enabled(const std::string& id, bool enabled) {
    auto it = std::find_if(m_options.begin(), m_options.end(),
                           [&id](const RadioOption& opt) { return opt.id == id; });
    if (it != m_options.end()) {
        it->enabled = enabled;
        mark_dirty();
    }
}

bool UIRadioGroup::is_option_enabled(const std::string& id) const {
    auto it = std::find_if(m_options.begin(), m_options.end(),
                           [&id](const RadioOption& opt) { return opt.id == id; });
    return it != m_options.end() && it->enabled;
}

std::string UIRadioGroup::get_resolved_label(const RadioOption& option) const {
    if (!option.label_key.empty()) {
        return engine::localization::loc(option.label_key);
    }
    return option.label;
}

int UIRadioGroup::get_option_at_position(Vec2 pos) const {
    for (size_t i = 0; i < m_option_layouts.size(); ++i) {
        if (m_option_layouts[i].total_bounds.contains(pos)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void UIRadioGroup::update_option_layouts() {
    m_option_layouts.resize(m_options.size());

    Rect bounds = get_bounds();
    float x = bounds.x;
    float y = bounds.y;

    for (size_t i = 0; i < m_options.size(); ++i) {
        OptionLayout& layout = m_option_layouts[i];

        // Radio indicator bounds (circle)
        layout.radio_bounds = Rect(x, y + 1.0f, m_radio_size, m_radio_size);

        // Label bounds
        float label_x = x + m_radio_size + m_label_padding;
        layout.label_bounds = Rect(label_x, y, bounds.width - m_radio_size - m_label_padding, m_radio_size);

        // Total clickable area
        float item_height = m_radio_size;
        layout.total_bounds = Rect(x, y, bounds.width, item_height);

        // Move to next position
        if (m_orientation == LayoutDirection::Vertical) {
            y += item_height + m_spacing;
        } else {
            // For horizontal, we'd need to measure text width
            // For simplicity, use a fixed width per option
            x += bounds.width / static_cast<float>(m_options.size());
        }
    }
}

void UIRadioGroup::on_update(float dt, const UIInputState& input) {
    if (!is_enabled()) {
        m_hovered_index = -1;
        return;
    }

    // Update hover state
    m_hovered_index = get_option_at_position(input.mouse_position);

    // Handle click
    if (input.was_mouse_pressed(0) && m_hovered_index >= 0) {
        const RadioOption& option = m_options[m_hovered_index];
        if (option.enabled) {
            set_selected(option.id);
        }
    }

    // Handle keyboard navigation when focused
    if (is_focused()) {
        int current_index = get_selected_index();

        if (input.was_nav_pressed(NavDirection::Up) || input.was_nav_pressed(NavDirection::Left)) {
            // Move to previous option
            for (int i = current_index - 1; i >= 0; --i) {
                if (m_options[i].enabled) {
                    set_selected(m_options[i].id);
                    break;
                }
            }
        } else if (input.was_nav_pressed(NavDirection::Down) || input.was_nav_pressed(NavDirection::Right)) {
            // Move to next option
            for (int i = current_index + 1; i < static_cast<int>(m_options.size()); ++i) {
                if (m_options[i].enabled) {
                    set_selected(m_options[i].id);
                    break;
                }
            }
        }
    }
}

void UIRadioGroup::on_render(UIRenderContext& ctx) {
    if (m_option_layouts.size() != m_options.size()) {
        update_option_layouts();
    }

    for (size_t i = 0; i < m_options.size(); ++i) {
        const RadioOption& option = m_options[i];
        const OptionLayout& layout = m_option_layouts[i];
        bool selected = (option.id == m_selected_id);
        bool hovered = (static_cast<int>(i) == m_hovered_index && option.enabled);

        // Determine colors
        Vec4 radio_bg = option.enabled ? m_radio_color : m_radio_disabled_color;
        Vec4 radio_border = m_radio_border_color;
        Vec4 text_color = option.enabled ? get_style().text_color.normal : Vec4(0.5f, 0.5f, 0.5f, 0.5f);

        if (hovered) {
            radio_border = m_radio_selected_color;
        }

        // Draw radio circle background
        float cx = layout.radio_bounds.x + layout.radio_bounds.width * 0.5f;
        float cy = layout.radio_bounds.y + layout.radio_bounds.height * 0.5f;
        float radius = layout.radio_bounds.width * 0.5f;
        Rect circle_rect(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

        ctx.draw_rect_rounded(circle_rect, radio_bg, radius);
        ctx.draw_rect_outline_rounded(circle_rect, radio_border, 2.0f, radius);

        // Draw selection indicator (inner filled circle)
        if (selected) {
            float inner_radius = radius * 0.5f;
            Rect inner_rect(cx - inner_radius, cy - inner_radius, inner_radius * 2.0f, inner_radius * 2.0f);
            ctx.draw_rect_rounded(inner_rect, m_radio_selected_color, inner_radius);
        }

        // Draw label
        FontHandle font = ctx.get_font_manager()->get_default_font();
        float font_size = 14.0f;
        float label_y = layout.label_bounds.center().y + font_size * 0.3f;

        std::string label = get_resolved_label(option);
        ctx.draw_text(label, Vec2(layout.label_bounds.x, label_y), font, font_size,
                      text_color, HAlign::Left);
    }
}

Vec2 UIRadioGroup::on_measure(Vec2 available_size) {
    float width = 0.0f;
    float height = 0.0f;

    // Calculate total size based on options
    for (size_t i = 0; i < m_options.size(); ++i) {
        float item_height = m_radio_size;
        float item_width = m_radio_size + m_label_padding + 100.0f; // Estimate label width

        if (m_orientation == LayoutDirection::Vertical) {
            width = std::max(width, item_width);
            height += item_height;
            if (i < m_options.size() - 1) {
                height += m_spacing;
            }
        } else {
            width += item_width;
            if (i < m_options.size() - 1) {
                width += m_spacing;
            }
            height = std::max(height, item_height);
        }
    }

    return Vec2(width, height);
}

void UIRadioGroup::on_layout(const Rect& bounds) {
    update_option_layouts();
}

} // namespace engine::ui
