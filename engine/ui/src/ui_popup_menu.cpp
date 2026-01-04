#include <engine/ui/ui_popup_menu.hpp>
#include <engine/ui/ui_renderer.hpp>
#include <engine/localization/localization.hpp>
#include <algorithm>
#include <cmath>

namespace engine::ui {

UIPopupMenu::UIPopupMenu() {
    set_visible(false);
}

void UIPopupMenu::add_item(const std::string& id, const std::string& label) {
    PopupMenuItem item;
    item.id = id;
    item.label = label;
    add_item(item);
}

void UIPopupMenu::add_item(const std::string& id, const std::string& label, const std::string& shortcut) {
    PopupMenuItem item;
    item.id = id;
    item.label = label;
    item.shortcut_text = shortcut;
    add_item(item);
}

void UIPopupMenu::add_item(const PopupMenuItem& item) {
    m_items.push_back(item);
}

void UIPopupMenu::add_separator() {
    m_items.push_back(PopupMenuItem::separator());
}

void UIPopupMenu::add_submenu(const std::string& label, std::vector<PopupMenuItem> items) {
    m_items.push_back(PopupMenuItem::submenu(label, std::move(items)));
}

void UIPopupMenu::clear_items() {
    m_items.clear();
    m_item_layouts.clear();
}

void UIPopupMenu::set_item_enabled(const std::string& id, bool enabled) {
    for (auto& item : m_items) {
        if (item.id == id) {
            item.enabled = enabled;
            return;
        }
    }
}

bool UIPopupMenu::is_item_enabled(const std::string& id) const {
    for (const auto& item : m_items) {
        if (item.id == id) {
            return item.enabled;
        }
    }
    return false;
}

void UIPopupMenu::set_item_checked(const std::string& id, bool checked) {
    for (auto& item : m_items) {
        if (item.id == id) {
            item.checked = checked;
            return;
        }
    }
}

bool UIPopupMenu::is_item_checked(const std::string& id) const {
    for (const auto& item : m_items) {
        if (item.id == id) {
            return item.checked;
        }
    }
    return false;
}

std::string UIPopupMenu::get_resolved_label(const PopupMenuItem& item) const {
    if (!item.label_key.empty()) {
        return engine::localization::loc(item.label_key);
    }
    return item.label;
}

void UIPopupMenu::calculate_layout() {
    m_item_layouts.clear();
    m_item_layouts.reserve(m_items.size());

    float y = m_position.y + m_padding;
    float max_width = m_min_width;

    // First pass: calculate width needed
    for (const auto& item : m_items) {
        if (item.type != MenuItemType::Separator) {
            // Estimate text width (would ideally use font metrics)
            float label_width = get_resolved_label(item).length() * 8.0f + 24.0f; // checkmark space
            float shortcut_width = item.shortcut_text.length() * 7.0f;
            float total_width = label_width + shortcut_width + m_padding * 4;

            if (item.type == MenuItemType::Submenu) {
                total_width += m_submenu_arrow_width;
            }

            max_width = std::max(max_width, total_width);
        }
    }

    // Second pass: create layouts
    for (const auto& item : m_items) {
        ItemLayout layout;
        layout.is_separator = (item.type == MenuItemType::Separator);

        float item_height = layout.is_separator ? m_separator_height : m_item_height;
        layout.bounds = Rect(m_position.x + m_padding, y, max_width - m_padding * 2, item_height);

        m_item_layouts.push_back(layout);
        y += item_height;
    }

    // Set total bounds
    float total_height = y - m_position.y + m_padding;
    set_size(Vec2(max_width, total_height));
    set_position(Vec2(m_position.x, m_position.y));
}

int UIPopupMenu::get_item_at_position(Vec2 pos) const {
    for (size_t i = 0; i < m_item_layouts.size(); ++i) {
        if (!m_item_layouts[i].is_separator && m_item_layouts[i].bounds.contains(pos)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void UIPopupMenu::show_at(Vec2 screen_position) {
    m_position = screen_position;
    m_visible = true;
    set_visible(true);
    calculate_layout();
    m_hovered_index = -1;
    close_submenu();
}

void UIPopupMenu::hide() {
    m_visible = false;
    set_visible(false);
    close_submenu();

    if (on_dismissed) {
        on_dismissed();
    }
}

void UIPopupMenu::close_submenu() {
    if (m_active_submenu) {
        m_active_submenu->hide();
        m_active_submenu.reset();
        m_submenu_parent_index = -1;
    }
}

void UIPopupMenu::open_submenu(int index, const PopupMenuItem& item) {
    if (m_submenu_parent_index == index) {
        return; // Already open
    }

    close_submenu();

    if (item.submenu_items.empty()) {
        return;
    }

    m_active_submenu = std::make_unique<UIPopupMenu>();

    // Copy visual settings
    m_active_submenu->m_bg_color = m_bg_color;
    m_active_submenu->m_hover_color = m_hover_color;
    m_active_submenu->m_separator_color = m_separator_color;
    m_active_submenu->m_text_color = m_text_color;
    m_active_submenu->m_disabled_text_color = m_disabled_text_color;
    m_active_submenu->m_shortcut_color = m_shortcut_color;
    m_active_submenu->m_item_height = m_item_height;

    // Add submenu items
    for (const auto& subitem : item.submenu_items) {
        m_active_submenu->add_item(subitem);
    }

    // Forward selection callback
    m_active_submenu->on_item_selected = on_item_selected;

    // Position submenu to the right of the parent item
    const ItemLayout& layout = m_item_layouts[index];
    Vec2 submenu_pos(layout.bounds.right() - 4.0f, layout.bounds.top());
    m_active_submenu->show_at(submenu_pos);

    m_submenu_parent_index = index;
}

void UIPopupMenu::on_update(float dt, const UIInputState& input) {
    if (!m_visible) {
        return;
    }

    // Update hover state
    int prev_hovered = m_hovered_index;
    m_hovered_index = get_item_at_position(input.mouse_position);

    // Handle submenu opening on hover
    if (m_hovered_index >= 0 && m_hovered_index != prev_hovered) {
        const PopupMenuItem& item = m_items[m_hovered_index];
        if (item.type == MenuItemType::Submenu) {
            open_submenu(m_hovered_index, item);
        } else if (m_submenu_parent_index != m_hovered_index) {
            close_submenu();
        }
    }

    // Update active submenu
    if (m_active_submenu) {
        m_active_submenu->on_update(dt, input);

        // If submenu handled a selection, we should close too
        if (!m_active_submenu->is_visible()) {
            close_submenu();
            hide();
            return;
        }
    }

    // Handle click
    if (input.was_mouse_pressed(0)) {
        if (m_hovered_index >= 0) {
            const PopupMenuItem& item = m_items[m_hovered_index];

            if (item.type == MenuItemType::Normal && item.enabled) {
                if (on_item_selected) {
                    on_item_selected(item.id);
                }
                hide();
            }
            // Submenu clicks are handled by submenu itself
        } else {
            // Click outside menu - close it
            Rect bounds = get_bounds();
            if (!bounds.contains(input.mouse_position)) {
                // Also check if click is inside submenu
                bool in_submenu = m_active_submenu &&
                                  m_active_submenu->get_bounds().contains(input.mouse_position);
                if (!in_submenu) {
                    hide();
                }
            }
        }
    }

    // Handle escape to close
    if (input.key_escape) {
        hide();
    }
}

void UIPopupMenu::on_render(UIRenderContext& ctx) {
    if (!m_visible) {
        return;
    }

    Rect bounds = get_bounds();

    // Draw background with border
    // Note: draw_rect_rounded args (rect, color, radius)
    ctx.draw_rect_rounded(bounds, m_bg_color, m_border_radius);
    // Note: draw_rect_outline_rounded args (rect, color, thickness, radius)
    ctx.draw_rect_outline_rounded(bounds, Vec4(0.3f, 0.3f, 0.3f, 1.0f), 1.0f, m_border_radius);

    FontHandle font = ctx.get_font_manager()->get_default_font();
    float font_size = 14.0f;
    float y_offset = font_size * 0.3f; // Approximate centering correction

    // Draw items
    for (size_t i = 0; i < m_items.size(); ++i) {
        const PopupMenuItem& item = m_items[i];
        const ItemLayout& layout = m_item_layouts[i];

        if (layout.is_separator) {
            // Draw separator line using rect
            float y = layout.bounds.center().y;
            Rect line_rect(layout.bounds.x + 8.0f, y - 0.5f, layout.bounds.width - 16.0f, 1.0f);
            ctx.draw_rect(line_rect, m_separator_color);
            continue;
        }

        bool hovered = (static_cast<int>(i) == m_hovered_index);
        bool is_submenu_parent = (static_cast<int>(i) == m_submenu_parent_index);

        // Draw hover highlight
        if ((hovered || is_submenu_parent) && item.enabled) {
            ctx.draw_rect_rounded(layout.bounds, m_hover_color, 2.0f);
        }

        // Determine text color
        Vec4 text_color = item.enabled ? m_text_color : m_disabled_text_color;

        // Draw checkmark if checked
        float text_x = layout.bounds.x + 8.0f;
        float text_y = layout.bounds.center().y + y_offset;
        
        if (item.checked) {
            ctx.draw_text("\xE2\x9C\x93", Vec2(text_x, text_y), font, font_size,
                         m_check_color, HAlign::Left);
        }
        text_x += 20.0f; // Space for checkmark

        // Draw label
        std::string label = get_resolved_label(item);
        ctx.draw_text(label, Vec2(text_x, text_y), font, font_size,
                      text_color, HAlign::Left);

        // Draw shortcut text
        if (!item.shortcut_text.empty()) {
            ctx.draw_text(item.shortcut_text, Vec2(layout.bounds.right() - 8.0f, text_y), 
                         font, font_size, m_shortcut_color, HAlign::Right);
        }

        // Draw submenu arrow
        if (item.type == MenuItemType::Submenu) {
            ctx.draw_text("\xE2\x96\xB6", Vec2(layout.bounds.right() - 12.0f, text_y), 
                         font, font_size, text_color, HAlign::Right);
        }
    }
}

void UIPopupMenu::render(UIRenderContext& ctx) {
    // Render self
    on_render(ctx);

    // Render active submenu (on top)
    if (m_active_submenu && m_active_submenu->is_visible()) {
        m_active_submenu->render(ctx);
    }
}

Vec2 UIPopupMenu::on_measure(Vec2 available_size) {
    calculate_layout();
    return get_size();
}

} // namespace engine::ui
