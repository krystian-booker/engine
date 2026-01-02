#pragma once

#include <engine/ui/ui_types.hpp>
#include <engine/ui/ui_style.hpp>
#include <memory>
#include <vector>
#include <string>

namespace engine::ui {

class UIRenderContext;

// Base class for all UI elements
class UIElement {
public:
    UIElement();
    virtual ~UIElement();

    // Non-copyable, movable
    UIElement(const UIElement&) = delete;
    UIElement& operator=(const UIElement&) = delete;
    UIElement(UIElement&&) = default;
    UIElement& operator=(UIElement&&) = default;

    // Identification
    void set_name(const std::string& name) { m_name = name; }
    const std::string& get_name() const { return m_name; }

    // Transform
    void set_anchor(Anchor anchor) { m_anchor = anchor; mark_layout_dirty(); }
    Anchor get_anchor() const { return m_anchor; }

    void set_position(Vec2 pos) { m_position = pos; mark_layout_dirty(); }
    Vec2 get_position() const { return m_position; }

    void set_size(Vec2 size) { m_size = size; mark_layout_dirty(); }
    Vec2 get_size() const { return m_size; }

    void set_pivot(Vec2 pivot) { m_pivot = pivot; mark_layout_dirty(); }
    Vec2 get_pivot() const { return m_pivot; }

    // Final computed bounds (after layout)
    const Rect& get_bounds() const { return m_bounds; }
    const Rect& get_content_bounds() const { return m_content_bounds; }

    // Style
    void set_style(const UIStyle& style) { m_style = style; mark_dirty(); }
    const UIStyle& get_style() const { return m_style; }
    UIStyle& style() { mark_dirty(); return m_style; }

    void add_class(const std::string& class_name);
    void remove_class(const std::string& class_name);
    bool has_class(const std::string& class_name) const;
    const std::vector<std::string>& get_classes() const { return m_classes; }

    // Hierarchy
    void add_child(std::unique_ptr<UIElement> child);
    void remove_child(UIElement* child);
    void remove_all_children();
    UIElement* get_parent() { return m_parent; }
    const UIElement* get_parent() const { return m_parent; }
    const std::vector<std::unique_ptr<UIElement>>& get_children() const { return m_children; }

    // Find child by name (recursive)
    UIElement* find_child(const std::string& name);

    // Visibility
    void set_visible(bool visible) { m_visible = visible; }
    bool is_visible() const { return m_visible; }

    // Enabled state
    void set_enabled(bool enabled) { m_enabled = enabled; mark_dirty(); }
    bool is_enabled() const { return m_enabled; }

    // Interactivity
    void set_interactive(bool interactive) { m_interactive = interactive; }
    bool is_interactive() const { return m_interactive; }

    // Focus
    bool is_focusable() const { return m_focusable; }
    bool is_focused() const { return m_focused; }
    void set_focusable(bool focusable) { m_focusable = focusable; }
    void request_focus();
    void release_focus();

    // Event callbacks
    ClickCallback on_click;
    HoverCallback on_hover;

    // Update and render (called by UICanvas)
    void update(float dt, const UIInputState& input);
    void render(UIRenderContext& ctx);

    // Layout
    void layout(const Rect& parent_bounds);
    Vec2 measure(Vec2 available_size);
    void mark_layout_dirty();
    void mark_dirty();

    // Hit testing
    bool hit_test(Vec2 point) const;
    UIElement* find_element_at(Vec2 point);

    // Current interaction state
    StyleState get_current_state() const;
    bool is_hovered() const { return m_hovered; }
    bool is_pressed() const { return m_pressed; }

protected:
    // Override in derived classes
    virtual void on_update(float dt, const UIInputState& input) {}
    virtual void on_render(UIRenderContext& ctx);
    virtual Vec2 on_measure(Vec2 available_size);
    virtual void on_layout(const Rect& bounds) {}
    virtual void on_click_internal() {}
    virtual void on_focus_changed(bool focused) {}

    // Helper to render background and border
    void render_background(UIRenderContext& ctx, const Rect& bounds);

    std::string m_name;
    UIStyle m_style;
    std::vector<std::string> m_classes;

    // Transform
    Anchor m_anchor = Anchor::TopLeft;
    Vec2 m_position{0.0f};
    Vec2 m_size{100.0f, 100.0f};
    Vec2 m_pivot{0.0f};

    // Computed layout
    Rect m_bounds;
    Rect m_content_bounds;
    bool m_layout_dirty = true;
    bool m_dirty = true;

    // State
    bool m_visible = true;
    bool m_enabled = true;
    bool m_interactive = false;
    bool m_focusable = false;
    bool m_focused = false;
    bool m_hovered = false;
    bool m_pressed = false;

    // Hierarchy
    UIElement* m_parent = nullptr;
    std::vector<std::unique_ptr<UIElement>> m_children;
};

} // namespace engine::ui
