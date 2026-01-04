#pragma once

#include <engine/ui/ui_element.hpp>
#include <functional>
#include <vector>
#include <any>

namespace engine::ui {

// Selection mode for list/grid
enum class SelectionMode : uint8_t {
    None,       // No selection
    Single,     // Single item selection
    Multiple    // Multiple item selection
};

// Data item wrapper for list/grid (type-erased)
struct ListItemData {
    std::any data;
    std::string id;

    ListItemData() = default;

    template<typename T>
    explicit ListItemData(T&& value, const std::string& item_id = "")
        : data(std::forward<T>(value)), id(item_id) {}

    template<typename T>
    const T* get() const {
        return std::any_cast<T>(&data);
    }
};

// Callback types for list/grid
using ItemRenderCallback = std::function<void(UIRenderContext& ctx, const Rect& bounds,
                                               const ListItemData& item, size_t index,
                                               bool selected, bool hovered)>;
using ItemClickCallback = std::function<void(const ListItemData& item, size_t index)>;
using SelectionChangedCallback = std::function<void(const std::vector<size_t>& selected_indices)>;

// Virtualized list view for efficient scrolling through large item collections
class UIListView : public UIElement {
public:
    UIListView();
    ~UIListView() override = default;

    // Data management
    void set_items(std::vector<ListItemData> items);
    void add_item(ListItemData item);
    void remove_item(size_t index);
    void clear_items();
    size_t get_item_count() const { return m_items.size(); }
    const ListItemData* get_item(size_t index) const;

    // Layout configuration
    void set_item_height(float height) { m_item_height = height; mark_layout_dirty(); }
    float get_item_height() const { return m_item_height; }

    void set_item_spacing(float spacing) { m_item_spacing = spacing; mark_layout_dirty(); }
    float get_item_spacing() const { return m_item_spacing; }

    // Selection
    void set_selection_mode(SelectionMode mode) { m_selection_mode = mode; }
    SelectionMode get_selection_mode() const { return m_selection_mode; }

    void select_index(size_t index, bool add_to_selection = false);
    void deselect_index(size_t index);
    void clear_selection();
    void select_all();

    bool is_selected(size_t index) const;
    const std::vector<size_t>& get_selected_indices() const { return m_selected_indices; }
    size_t get_selected_count() const { return m_selected_indices.size(); }

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

    // Custom item rendering callback
    void set_item_renderer(ItemRenderCallback callback) { m_item_renderer = std::move(callback); }

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
    // Virtualization helpers
    size_t get_first_visible_index() const;
    size_t get_visible_count() const;
    Rect get_item_bounds(size_t index) const;
    int get_item_at_position(Vec2 pos) const;

    void render_scrollbar(UIRenderContext& ctx);
    void render_default_item(UIRenderContext& ctx, const Rect& bounds,
                              const ListItemData& item, size_t index,
                              bool selected, bool hovered);

    std::vector<ListItemData> m_items;
    float m_item_height = 32.0f;
    float m_item_spacing = 2.0f;
    float m_scroll_offset = 0.0f;
    float m_scrollbar_width = 8.0f;
    bool m_show_scrollbar = true;

    SelectionMode m_selection_mode = SelectionMode::Single;
    std::vector<size_t> m_selected_indices;
    int m_hovered_index = -1;

    ItemRenderCallback m_item_renderer;

    // Double-click tracking
    float m_last_click_time = 0.0f;
    int m_last_click_index = -1;
    static constexpr float DOUBLE_CLICK_TIME = 0.3f;
};

} // namespace engine::ui
