#pragma once

#include <engine/ui/ui_element.hpp>
#include <string>
#include <vector>
#include <functional>

namespace engine::ui {

// Tab item definition
struct TabItem {
    std::string id;           // Unique identifier
    std::string label;        // Display text
    std::string label_key;    // Localization key (optional, overrides label if set)
};

// Tab bar position relative to content
enum class TabPosition {
    Top,
    Bottom,
    Left,
    Right
};

// Tab control for tabbed interfaces (options menus, inventory categories, etc.)
class UITabControl : public UIElement {
public:
    UITabControl();

    // Tab management
    void add_tab(const std::string& id, const std::string& label);
    void add_tab_localized(const std::string& id, const std::string& label_key);
    void remove_tab(const std::string& id);
    void clear_tabs();
    const std::vector<TabItem>& get_tabs() const { return m_tabs; }
    size_t get_tab_count() const { return m_tabs.size(); }

    // Selection
    void set_selected_tab(const std::string& id);
    void set_selected_index(int index);
    const std::string& get_selected_tab() const { return m_selected_id; }
    int get_selected_index() const;

    // Content access (child at index corresponds to tab at index)
    UIElement* get_tab_content(const std::string& id);
    UIElement* get_tab_content(int index);
    UIElement* get_active_content();

    // Tab bar positioning
    void set_tab_position(TabPosition pos) { m_tab_position = pos; mark_layout_dirty(); }
    TabPosition get_tab_position() const { return m_tab_position; }

    // Tab bar styling
    void set_tab_height(float height) { m_tab_height = height; mark_layout_dirty(); }
    float get_tab_height() const { return m_tab_height; }

    void set_tab_spacing(float spacing) { m_tab_spacing = spacing; mark_layout_dirty(); }
    float get_tab_spacing() const { return m_tab_spacing; }

    void set_tab_padding(float padding) { m_tab_padding = padding; mark_layout_dirty(); }
    float get_tab_padding() const { return m_tab_padding; }

    // Tab colors
    void set_tab_color(const Vec4& color) { m_tab_color = color; }
    void set_tab_selected_color(const Vec4& color) { m_tab_selected_color = color; }
    void set_tab_hover_color(const Vec4& color) { m_tab_hover_color = color; }
    void set_tab_text_color(const Vec4& color) { m_tab_text_color = color; }

    // Callback when tab selection changes
    std::function<void(const std::string& id, int index)> on_tab_changed;

protected:
    void on_update(float dt, const UIInputState& input) override;
    void on_render(UIRenderContext& ctx) override;
    Vec2 on_measure(Vec2 available_size) override;
    void on_layout(const Rect& bounds) override;

private:
    std::string get_resolved_label(const TabItem& tab) const;
    void render_tab_bar(UIRenderContext& ctx);
    void render_tab(UIRenderContext& ctx, const TabItem& tab, const Rect& bounds,
                    bool selected, bool hovered);
    int get_tab_at_position(Vec2 pos) const;
    void update_content_visibility();
    void compute_tab_bounds();
    float get_tab_bar_size() const;
    bool is_horizontal() const;

    std::vector<TabItem> m_tabs;
    std::string m_selected_id;
    int m_hovered_tab = -1;
    int m_pressed_tab = -1;

    TabPosition m_tab_position = TabPosition::Top;
    float m_tab_height = 32.0f;
    float m_tab_spacing = 2.0f;
    float m_tab_padding = 12.0f;

    // Tab colors
    Vec4 m_tab_color{0.15f, 0.15f, 0.15f, 1.0f};
    Vec4 m_tab_selected_color{0.25f, 0.25f, 0.30f, 1.0f};
    Vec4 m_tab_hover_color{0.20f, 0.20f, 0.25f, 1.0f};
    Vec4 m_tab_text_color{0.9f, 0.9f, 0.9f, 1.0f};

    // Computed bounds
    Rect m_tab_bar_bounds;
    Rect m_content_bounds;
    std::vector<Rect> m_tab_bounds;
};

} // namespace engine::ui
