#include <engine/ui/ui_list_view.hpp>
#include <engine/ui/ui_renderer.hpp>
#include <engine/ui/ui_context.hpp>
#include <algorithm>
#include <cmath>

namespace engine::ui {

UIListView::UIListView() {
    m_style = UIStyle::panel();
    m_interactive = true;
}

void UIListView::set_items(std::vector<ListItemData> items) {
    m_items = std::move(items);
    m_scroll_offset = 0.0f;
    clear_selection();
    mark_layout_dirty();
}

void UIListView::add_item(ListItemData item) {
    m_items.push_back(std::move(item));
    mark_layout_dirty();
}

void UIListView::remove_item(size_t index) {
    if (index >= m_items.size()) return;

    m_items.erase(m_items.begin() + static_cast<ptrdiff_t>(index));

    // Update selection indices
    for (auto it = m_selected_indices.begin(); it != m_selected_indices.end();) {
        if (*it == index) {
            it = m_selected_indices.erase(it);
        } else {
            if (*it > index) --(*it);
            ++it;
        }
    }

    mark_layout_dirty();
}

void UIListView::clear_items() {
    m_items.clear();
    m_scroll_offset = 0.0f;
    clear_selection();
    mark_layout_dirty();
}

const ListItemData* UIListView::get_item(size_t index) const {
    if (index >= m_items.size()) return nullptr;
    return &m_items[index];
}

void UIListView::select_index(size_t index, bool add_to_selection) {
    if (index >= m_items.size()) return;
    if (m_selection_mode == SelectionMode::None) return;

    if (!add_to_selection || m_selection_mode == SelectionMode::Single) {
        m_selected_indices.clear();
    }

    auto it = std::find(m_selected_indices.begin(), m_selected_indices.end(), index);
    if (it == m_selected_indices.end()) {
        m_selected_indices.push_back(index);
        if (on_selection_changed) {
            on_selection_changed(m_selected_indices);
        }
    }

    mark_dirty();
}

void UIListView::deselect_index(size_t index) {
    auto it = std::find(m_selected_indices.begin(), m_selected_indices.end(), index);
    if (it != m_selected_indices.end()) {
        m_selected_indices.erase(it);
        if (on_selection_changed) {
            on_selection_changed(m_selected_indices);
        }
        mark_dirty();
    }
}

void UIListView::clear_selection() {
    if (!m_selected_indices.empty()) {
        m_selected_indices.clear();
        if (on_selection_changed) {
            on_selection_changed(m_selected_indices);
        }
        mark_dirty();
    }
}

void UIListView::select_all() {
    if (m_selection_mode != SelectionMode::Multiple) return;

    m_selected_indices.clear();
    for (size_t i = 0; i < m_items.size(); ++i) {
        m_selected_indices.push_back(i);
    }

    if (on_selection_changed) {
        on_selection_changed(m_selected_indices);
    }
    mark_dirty();
}

bool UIListView::is_selected(size_t index) const {
    return std::find(m_selected_indices.begin(), m_selected_indices.end(), index)
           != m_selected_indices.end();
}

void UIListView::scroll_to_index(size_t index) {
    if (index >= m_items.size()) return;

    float item_top = static_cast<float>(index) * (m_item_height + m_item_spacing);
    float item_bottom = item_top + m_item_height;
    float visible_top = m_scroll_offset;
    float visible_bottom = m_scroll_offset + m_content_bounds.height;

    if (item_top < visible_top) {
        set_scroll_offset(item_top);
    } else if (item_bottom > visible_bottom) {
        set_scroll_offset(item_bottom - m_content_bounds.height);
    }
}

void UIListView::scroll_to_top() {
    set_scroll_offset(0.0f);
}

void UIListView::scroll_to_bottom() {
    set_scroll_offset(get_max_scroll());
}

void UIListView::set_scroll_offset(float offset) {
    float max_scroll = get_max_scroll();
    offset = std::clamp(offset, 0.0f, max_scroll);
    if (m_scroll_offset != offset) {
        m_scroll_offset = offset;
        mark_dirty();
    }
}

float UIListView::get_max_scroll() const {
    float total_height = static_cast<float>(m_items.size()) * (m_item_height + m_item_spacing);
    if (!m_items.empty()) {
        total_height -= m_item_spacing; // No spacing after last item
    }
    return std::max(0.0f, total_height - m_content_bounds.height);
}

size_t UIListView::get_first_visible_index() const {
    if (m_items.empty()) return 0;
    float item_total = m_item_height + m_item_spacing;
    return static_cast<size_t>(std::floor(m_scroll_offset / item_total));
}

size_t UIListView::get_visible_count() const {
    if (m_items.empty()) return 0;
    float item_total = m_item_height + m_item_spacing;
    return static_cast<size_t>(std::ceil(m_content_bounds.height / item_total)) + 1;
}

Rect UIListView::get_item_bounds(size_t index) const {
    float item_total = m_item_height + m_item_spacing;
    float y = m_content_bounds.y + static_cast<float>(index) * item_total - m_scroll_offset;

    float width = m_content_bounds.width;
    if (m_show_scrollbar && get_max_scroll() > 0.0f) {
        width -= m_scrollbar_width;
    }

    return Rect(m_content_bounds.x, y, width, m_item_height);
}

int UIListView::get_item_at_position(Vec2 pos) const {
    if (!m_content_bounds.contains(pos)) return -1;

    float rel_y = pos.y - m_content_bounds.y + m_scroll_offset;
    float item_total = m_item_height + m_item_spacing;
    int index = static_cast<int>(std::floor(rel_y / item_total));

    if (index < 0 || static_cast<size_t>(index) >= m_items.size()) {
        return -1;
    }

    // Check if within item (not in spacing)
    float item_top = static_cast<float>(index) * item_total;
    if (rel_y - item_top > m_item_height) {
        return -1; // In spacing area
    }

    return index;
}

void UIListView::on_update(float dt, const UIInputState& input) {
    m_last_click_time += dt;

    // Handle mouse wheel scrolling
    if (is_hovered() && input.scroll_delta.y != 0.0f) {
        float scroll_speed = m_item_height + m_item_spacing;
        set_scroll_offset(m_scroll_offset - input.scroll_delta.y * scroll_speed);
    }

    // Update hovered item
    m_hovered_index = get_item_at_position(input.mouse_position);

    // Handle click
    if (m_hovered_index >= 0 && input.was_mouse_released(0)) {
        size_t index = static_cast<size_t>(m_hovered_index);

        // Check for double-click
        if (m_last_click_index == m_hovered_index && m_last_click_time < DOUBLE_CLICK_TIME) {
            if (on_item_double_clicked) {
                on_item_double_clicked(m_items[index], index);
            }
            m_last_click_index = -1;
        } else {
            // Single click - handle selection
            if (m_selection_mode != SelectionMode::None) {
                bool ctrl_held = false; // Would need keyboard state
                if (m_selection_mode == SelectionMode::Multiple && ctrl_held) {
                    if (is_selected(index)) {
                        deselect_index(index);
                    } else {
                        select_index(index, true);
                    }
                } else {
                    select_index(index, false);
                }
            }

            if (on_item_clicked) {
                on_item_clicked(m_items[index], index);
            }

            m_last_click_index = m_hovered_index;
            m_last_click_time = 0.0f;
        }
    }
}

void UIListView::render(UIRenderContext& ctx) {
    if (!is_visible()) return;

    on_render(ctx);

    // Render visible items
    size_t first = get_first_visible_index();
    size_t count = get_visible_count();
    size_t last = std::min(first + count, m_items.size());

    for (size_t i = first; i < last; ++i) {
        Rect bounds = get_item_bounds(i);

        // Skip if completely outside visible area
        if (bounds.bottom() < m_content_bounds.y ||
            bounds.y > m_content_bounds.bottom()) {
            continue;
        }

        bool selected = is_selected(i);
        bool hovered = (static_cast<int>(i) == m_hovered_index);

        if (m_item_renderer) {
            m_item_renderer(ctx, bounds, m_items[i], i, selected, hovered);
        } else {
            render_default_item(ctx, bounds, m_items[i], i, selected, hovered);
        }
    }

    // Render scrollbar
    if (m_show_scrollbar && get_max_scroll() > 0.0f) {
        render_scrollbar(ctx);
    }

    ctx.pop_clip_rect();
    m_dirty = false;
}

void UIListView::on_render(UIRenderContext& ctx) {
    render_background(ctx, m_bounds);
    ctx.push_clip_rect(m_content_bounds);
}

void UIListView::render_scrollbar(UIRenderContext& ctx) {
    float max_scroll = get_max_scroll();
    if (max_scroll <= 0.0f) return;

    float total_height = static_cast<float>(m_items.size()) * (m_item_height + m_item_spacing);
    float track_height = m_content_bounds.height;
    float visible_ratio = m_content_bounds.height / total_height;
    float thumb_height = std::max(20.0f, track_height * visible_ratio);
    float scroll_ratio = m_scroll_offset / max_scroll;
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

void UIListView::render_default_item(UIRenderContext& ctx, const Rect& bounds,
                                      const ListItemData& item, size_t index,
                                      bool selected, bool hovered) {
    // Background
    Vec4 bg_color = Vec4(0.15f, 0.15f, 0.15f, 1.0f);
    if (selected) {
        bg_color = Vec4(0.3f, 0.5f, 0.8f, 1.0f);
    } else if (hovered) {
        bg_color = Vec4(0.25f, 0.25f, 0.25f, 1.0f);
    }
    ctx.draw_rect(bounds, bg_color);

    // Text (if item has a string representation)
    if (auto* str = item.get<std::string>()) {
        UIContext* ui_ctx = get_ui_context();
        if (ui_ctx) {
            Vec4 text_color = Vec4(1.0f);
            Vec2 text_pos(bounds.x + 8.0f, bounds.y + bounds.height * 0.5f);
            ctx.draw_text(*str, text_pos, ui_ctx->get_default_font(), 14.0f, text_color, HAlign::Left);
        }
    }
}

Vec2 UIListView::on_measure(Vec2 available_size) {
    return UIElement::on_measure(available_size);
}

void UIListView::on_layout(const Rect& bounds) {
    // Layout is handled dynamically during render (virtualization)
}

} // namespace engine::ui
