#include <engine/ui/ui_grid_view.hpp>
#include <engine/ui/ui_renderer.hpp>
#include <engine/ui/ui_context.hpp>
#include <algorithm>
#include <cmath>

namespace engine::ui {

UIGridView::UIGridView() {
    m_style = UIStyle::panel();
    m_interactive = true;
}

void UIGridView::set_items(std::vector<ListItemData> items) {
    m_items = std::move(items);
    m_scroll_offset = 0.0f;
    clear_selection();
    mark_layout_dirty();
}

void UIGridView::add_item(ListItemData item) {
    m_items.push_back(std::move(item));
    mark_layout_dirty();
}

void UIGridView::remove_item(size_t index) {
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

void UIGridView::clear_items() {
    m_items.clear();
    m_scroll_offset = 0.0f;
    clear_selection();
    mark_layout_dirty();
}

const ListItemData* UIGridView::get_item(size_t index) const {
    if (index >= m_items.size()) return nullptr;
    return &m_items[index];
}

int UIGridView::get_column_count() const {
    if (m_auto_columns) {
        return calculate_column_count();
    }
    return m_column_count;
}

int UIGridView::calculate_column_count() const {
    float available_width = m_content_bounds.width;
    if (m_show_scrollbar && get_max_scroll() > 0.0f) {
        available_width -= m_scrollbar_width;
    }

    if (available_width <= 0.0f) return 1;

    int cols = static_cast<int>((available_width + m_cell_spacing.x) / (m_cell_size.x + m_cell_spacing.x));
    return std::max(1, cols);
}

int UIGridView::get_row_count() const {
    if (m_items.empty()) return 0;
    int cols = get_column_count();
    return static_cast<int>((m_items.size() + cols - 1) / cols);
}

void UIGridView::select_index(size_t index, bool add_to_selection) {
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

void UIGridView::deselect_index(size_t index) {
    auto it = std::find(m_selected_indices.begin(), m_selected_indices.end(), index);
    if (it != m_selected_indices.end()) {
        m_selected_indices.erase(it);
        if (on_selection_changed) {
            on_selection_changed(m_selected_indices);
        }
        mark_dirty();
    }
}

void UIGridView::clear_selection() {
    if (!m_selected_indices.empty()) {
        m_selected_indices.clear();
        if (on_selection_changed) {
            on_selection_changed(m_selected_indices);
        }
        mark_dirty();
    }
}

bool UIGridView::is_selected(size_t index) const {
    return std::find(m_selected_indices.begin(), m_selected_indices.end(), index)
           != m_selected_indices.end();
}

void UIGridView::scroll_to_index(size_t index) {
    if (index >= m_items.size()) return;

    int cols = get_column_count();
    int row = static_cast<int>(index) / cols;
    float row_height = m_cell_size.y + m_cell_spacing.y;

    float item_top = static_cast<float>(row) * row_height;
    float item_bottom = item_top + m_cell_size.y;
    float visible_top = m_scroll_offset;
    float visible_bottom = m_scroll_offset + m_content_bounds.height;

    if (item_top < visible_top) {
        set_scroll_offset(item_top);
    } else if (item_bottom > visible_bottom) {
        set_scroll_offset(item_bottom - m_content_bounds.height);
    }
}

void UIGridView::scroll_to_top() {
    set_scroll_offset(0.0f);
}

void UIGridView::scroll_to_bottom() {
    set_scroll_offset(get_max_scroll());
}

void UIGridView::set_scroll_offset(float offset) {
    float max_scroll = get_max_scroll();
    offset = std::clamp(offset, 0.0f, max_scroll);
    if (m_scroll_offset != offset) {
        m_scroll_offset = offset;
        mark_dirty();
    }
}

float UIGridView::get_max_scroll() const {
    int row_count = get_row_count();
    float row_height = m_cell_size.y + m_cell_spacing.y;
    float total_height = static_cast<float>(row_count) * row_height;
    if (row_count > 0) {
        total_height -= m_cell_spacing.y; // No spacing after last row
    }
    return std::max(0.0f, total_height - m_content_bounds.height);
}

size_t UIGridView::get_first_visible_row() const {
    float row_height = m_cell_size.y + m_cell_spacing.y;
    return static_cast<size_t>(std::floor(m_scroll_offset / row_height));
}

size_t UIGridView::get_visible_row_count() const {
    float row_height = m_cell_size.y + m_cell_spacing.y;
    return static_cast<size_t>(std::ceil(m_content_bounds.height / row_height)) + 1;
}

Rect UIGridView::get_cell_bounds(size_t index) const {
    int cols = get_column_count();
    int row = static_cast<int>(index) / cols;
    int col = static_cast<int>(index) % cols;

    float x = m_content_bounds.x + static_cast<float>(col) * (m_cell_size.x + m_cell_spacing.x);
    float y = m_content_bounds.y + static_cast<float>(row) * (m_cell_size.y + m_cell_spacing.y) - m_scroll_offset;

    return Rect(x, y, m_cell_size.x, m_cell_size.y);
}

int UIGridView::get_cell_at_position(Vec2 pos) const {
    if (!m_content_bounds.contains(pos)) return -1;

    int cols = get_column_count();
    float cell_width = m_cell_size.x + m_cell_spacing.x;
    float cell_height = m_cell_size.y + m_cell_spacing.y;

    float rel_x = pos.x - m_content_bounds.x;
    float rel_y = pos.y - m_content_bounds.y + m_scroll_offset;

    int col = static_cast<int>(std::floor(rel_x / cell_width));
    int row = static_cast<int>(std::floor(rel_y / cell_height));

    if (col < 0 || col >= cols) return -1;

    // Check if within cell (not in spacing)
    float cell_local_x = rel_x - static_cast<float>(col) * cell_width;
    float cell_local_y = rel_y - static_cast<float>(row) * cell_height;

    if (cell_local_x > m_cell_size.x || cell_local_y > m_cell_size.y) {
        return -1; // In spacing area
    }

    int index = row * cols + col;
    if (index < 0 || static_cast<size_t>(index) >= m_items.size()) {
        return -1;
    }

    return index;
}

void UIGridView::on_update(float dt, const UIInputState& input) {
    m_last_click_time += dt;

    // Handle mouse wheel scrolling
    if (is_hovered() && input.scroll_delta.y != 0.0f) {
        float scroll_speed = m_cell_size.y + m_cell_spacing.y;
        set_scroll_offset(m_scroll_offset - input.scroll_delta.y * scroll_speed);
    }

    // Update hovered cell
    m_hovered_index = get_cell_at_position(input.mouse_position);

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

void UIGridView::render(UIRenderContext& ctx) {
    if (!is_visible()) return;

    on_render(ctx);

    // Render visible cells
    int cols = get_column_count();
    size_t first_row = get_first_visible_row();
    size_t visible_rows = get_visible_row_count();
    size_t first_index = first_row * cols;
    size_t last_row = std::min(first_row + visible_rows, static_cast<size_t>(get_row_count()));

    for (size_t row = first_row; row < last_row; ++row) {
        for (int col = 0; col < cols; ++col) {
            size_t index = row * cols + col;
            if (index >= m_items.size()) break;

            Rect bounds = get_cell_bounds(index);

            // Skip if completely outside visible area
            if (bounds.bottom() < m_content_bounds.y ||
                bounds.y > m_content_bounds.bottom()) {
                continue;
            }

            bool selected = is_selected(index);
            bool hovered = (static_cast<int>(index) == m_hovered_index);

            if (m_cell_renderer) {
                m_cell_renderer(ctx, bounds, m_items[index], index, selected, hovered);
            } else {
                render_default_cell(ctx, bounds, m_items[index], index, selected, hovered);
            }
        }
    }

    // Render scrollbar
    if (m_show_scrollbar && get_max_scroll() > 0.0f) {
        render_scrollbar(ctx);
    }

    ctx.pop_clip_rect();
    m_dirty = false;
}

void UIGridView::on_render(UIRenderContext& ctx) {
    render_background(ctx, m_bounds);
    ctx.push_clip_rect(m_content_bounds);
}

void UIGridView::render_scrollbar(UIRenderContext& ctx) {
    float max_scroll = get_max_scroll();
    if (max_scroll <= 0.0f) return;

    int row_count = get_row_count();
    float row_height = m_cell_size.y + m_cell_spacing.y;
    float total_height = static_cast<float>(row_count) * row_height;

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

void UIGridView::render_default_cell(UIRenderContext& ctx, const Rect& bounds,
                                      const ListItemData& item, size_t index,
                                      bool selected, bool hovered) {
    // Background
    Vec4 bg_color = Vec4(0.2f, 0.2f, 0.2f, 1.0f);
    if (selected) {
        bg_color = Vec4(0.3f, 0.5f, 0.8f, 1.0f);
    } else if (hovered) {
        bg_color = Vec4(0.3f, 0.3f, 0.3f, 1.0f);
    }
    ctx.draw_rect(bounds, bg_color);

    // Border
    Vec4 border_color = selected ? Vec4(0.5f, 0.7f, 1.0f, 1.0f) : Vec4(0.3f, 0.3f, 0.3f, 1.0f);
    ctx.draw_rect_outline(bounds, border_color, 1.0f);
}

Vec2 UIGridView::on_measure(Vec2 available_size) {
    return UIElement::on_measure(available_size);
}

void UIGridView::on_layout(const Rect& bounds) {
    // Layout is handled dynamically during render (virtualization)
}

} // namespace engine::ui
