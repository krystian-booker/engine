#pragma once

#include <engine/ui/ui_element.hpp>
#include <engine/ui/ui_renderer.hpp>
#include <engine/render/types.hpp>
#include <engine/render/render_target.hpp>
#include <memory>
#include <string>

namespace engine::ui {

// UI Canvas - container for UI elements that can render to screen or texture
class UICanvas {
public:
    UICanvas();
    virtual ~UICanvas();

    // Set root element
    void set_root(std::unique_ptr<UIElement> root);
    UIElement* get_root() { return m_root.get(); }
    const UIElement* get_root() const { return m_root.get(); }

    // Size
    void set_size(uint32_t width, uint32_t height);
    uint32_t get_width() const { return m_width; }
    uint32_t get_height() const { return m_height; }

    // Render target (nullptr = render to screen)
    void set_render_target(render::RenderTargetHandle target) { m_render_target = target; }
    render::RenderTargetHandle get_render_target() const { return m_render_target; }

    // Sorting order (higher = rendered on top)
    void set_sort_order(int order) { m_sort_order = order; }
    int get_sort_order() const { return m_sort_order; }

    // Scale mode
    enum class ScaleMode {
        ConstantPixelSize,    // UI stays same pixel size regardless of screen
        ScaleWithScreen,      // UI scales with screen size
        ConstantPhysicalSize  // UI stays same physical size (uses DPI)
    };

    void set_scale_mode(ScaleMode mode) { m_scale_mode = mode; }
    ScaleMode get_scale_mode() const { return m_scale_mode; }

    void set_reference_resolution(uint32_t width, uint32_t height);
    void set_pixels_per_unit(float ppu) { m_pixels_per_unit = ppu; }

    // Update (process input, animations)
    void update(float dt, const UIInputState& input);

    // Render to context
    void render(UIRenderContext& ctx);

    // Focus management
    void set_focused_element(UIElement* element);
    UIElement* get_focused_element() { return m_focused_element; }

    // Focus navigation
    void navigate_focus(NavDirection direction);
    void focus_next();
    void focus_previous();
    void activate_focused();  // Trigger click on focused element

    // Hit testing
    UIElement* find_element_at(Vec2 point);

    // Enable/disable canvas
    void set_enabled(bool enabled) { m_enabled = enabled; }
    bool is_enabled() const { return m_enabled; }

private:
    void layout_root();
    void collect_focusable_elements(UIElement* element, std::vector<UIElement*>& out);
    UIElement* find_nearest_in_direction(UIElement* from, NavDirection dir);

    std::unique_ptr<UIElement> m_root;

    uint32_t m_width = 1920;
    uint32_t m_height = 1080;

    render::RenderTargetHandle m_render_target;
    int m_sort_order = 0;

    ScaleMode m_scale_mode = ScaleMode::ConstantPixelSize;
    uint32_t m_reference_width = 1920;
    uint32_t m_reference_height = 1080;
    float m_pixels_per_unit = 100.0f;

    UIElement* m_focused_element = nullptr;
    UIElement* m_hovered_element = nullptr;

    bool m_enabled = true;
    bool m_layout_dirty = true;
};

} // namespace engine::ui
