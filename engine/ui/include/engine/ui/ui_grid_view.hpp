#pragma once

#include <engine/ui/ui_list_view.hpp>

namespace engine::ui {

// Callback type for grid item rendering
using GridItemRenderCallback = std::function<void(UIRenderContext& ctx, const Rect& bounds,
                                                   const ListItemData& item, size_t index,
                                                   bool selected, bool hovered)>;

// Virtualized grid view for inventory screens, icon grids, etc.
class UIGridView : public UIElement {
public:
    UIGridView();
    ~UIGridView() override = default;

    // Data management (same as ListView)
    void set_items(std::vector<ListItemData> items);
    void add_item(ListItemData item);
    void remove_item(size_t index);
    void clear_items();
    size_t get_item_count() const { return m_items.size(); }
    const ListItemData* get_item(size_t index) const;

    // Grid configuration
    void set_cell_size(Vec2 size) { m_cell_size = size; mark_layout_dirty(); }
    Vec2 get_cell_size() const { return m_cell_size; }

    void set_cell_spacing(Vec2 spacing) { m_cell_spacing = spacing; mark_layout_dirty(); }
    Vec2 get_cell_spacing() const { return m_cell_spacing; }

    // Auto-calculated column count based on width
    void set_auto_columns(bool auto_cols) { m_auto_columns = auto_cols; mark_layout_dirty(); }
    bool get_auto_columns() const { return m_auto_columns; }

    // Fixed column count (when auto_columns is false)
    void set_column_count(int columns) { m_column_count = columns; mark_layout_dirty(); }
    int get_column_count() const;

    // Selection
    void set_selection_mode(SelectionMode mode) { m_selection_mode = mode; }
    SelectionMode get_selection_mode() const { return m_selection_mode; }

    void select_index(size_t index, bool add_to_selection = false);
    void deselect_index(size_t index);
    void clear_selection();

    bool is_selected(size_t index) const;
    const std::vector<size_t>& get_selected_indices() const { return m_selected_indices; }

    // Scroll control
    void scroll_to_index(size_t index);
    void scroll_to_top();
    void scroll_to_bottom();

    float get_scroll_offset() const { return m_scroll_offset; }
    void set_scroll_offset(float offset);
    float get_max_scroll() const;

    // Scrollbar
    void set_show_scrollbar(bool show) { m_show_scrollbar = show; }
    bool get_show_scrollbar() const { return m_show_scrollbar; }

    // Custom cell rendering callback
    void set_cell_renderer(GridItemRenderCallback callback) { m_cell_renderer = std::move(callback); }

    // Callbacks
    ItemClickCallback on_item_clicked;
    ItemClickCallback on_item_double_clicked;
    SelectionChangedCallback on_selection_changed;

    void render(UIRenderContext& ctx) override;

protected:
    void on_update(float dt, const UIInputState& input) override;
    void on_render(UIRenderContext& ctx) override;
    Vec2 on_measure(Vec2 available_size) override;
    void on_layout(const Rect& bounds) override;

private:
    // Layout helpers
    int calculate_column_count() const;
    int get_row_count() const;
    size_t get_first_visible_row() const;
    size_t get_visible_row_count() const;
    Rect get_cell_bounds(size_t index) const;
    int get_cell_at_position(Vec2 pos) const;

    void render_scrollbar(UIRenderContext& ctx);
    void render_default_cell(UIRenderContext& ctx, const Rect& bounds,
                              const ListItemData& item, size_t index,
                              bool selected, bool hovered);

    std::vector<ListItemData> m_items;
    Vec2 m_cell_size{64.0f, 64.0f};
    Vec2 m_cell_spacing{4.0f, 4.0f};
    int m_column_count = 4;
    bool m_auto_columns = true;

    float m_scroll_offset = 0.0f;
    float m_scrollbar_width = 8.0f;
    bool m_show_scrollbar = true;

    SelectionMode m_selection_mode = SelectionMode::Single;
    std::vector<size_t> m_selected_indices;
    int m_hovered_index = -1;

    GridItemRenderCallback m_cell_renderer;

    // Double-click tracking
    float m_last_click_time = 0.0f;
    int m_last_click_index = -1;
    static constexpr float DOUBLE_CLICK_TIME = 0.3f;
};

} // namespace engine::ui
