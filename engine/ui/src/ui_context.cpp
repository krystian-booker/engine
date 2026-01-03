#include <engine/ui/ui_context.hpp>
#include <engine/core/log.hpp>
#include <algorithm>

namespace engine::ui {

// Global context pointer
static UIContext* g_ui_context = nullptr;

UIContext* get_ui_context() {
    return g_ui_context;
}

void set_ui_context(UIContext* ctx) {
    g_ui_context = ctx;
}

UIContext::UIContext() = default;
UIContext::~UIContext() {
    shutdown();
}

bool UIContext::init(render::IRenderer* renderer) {
    if (m_initialized) {
        return true;
    }

    m_render = renderer;

    // Initialize renderer
    if (!m_renderer.init()) {
        core::log(core::LogLevel::Error, "UIContext: Failed to initialize renderer");
        return false;
    }

    // Initialize font manager
    m_font_manager.init();

    // Set up render context
    m_render_context.set_font_manager(&m_font_manager);

    // Set default theme
    m_theme = UITheme::dark();

    m_initialized = true;
    core::log(core::LogLevel::Info, "UIContext initialized");

    return true;
}

void UIContext::shutdown() {
    if (!m_initialized) return;

    m_canvases.clear();
    m_canvas_order.clear();

    m_font_manager.shutdown();
    m_renderer.shutdown();

    m_render = nullptr;
    m_initialized = false;

    core::log(core::LogLevel::Info, "UIContext shutdown");
}

UICanvas* UIContext::create_canvas(const std::string& name) {
    if (m_canvases.find(name) != m_canvases.end()) {
        core::log(core::LogLevel::Warn, "UIContext: Canvas '{}' already exists", name);
        return m_canvases[name].get();
    }

    auto canvas = std::make_unique<UICanvas>();
    canvas->set_size(m_screen_width, m_screen_height);

    UICanvas* ptr = canvas.get();
    m_canvases[name] = std::move(canvas);
    m_canvas_order.push_back(ptr);

    sort_canvases();

    return ptr;
}

void UIContext::destroy_canvas(const std::string& name) {
    auto it = m_canvases.find(name);
    if (it == m_canvases.end()) {
        return;
    }

    UICanvas* canvas = it->second.get();

    // Remove from order list
    m_canvas_order.erase(
        std::remove(m_canvas_order.begin(), m_canvas_order.end(), canvas),
        m_canvas_order.end());

    m_canvases.erase(it);
}

UICanvas* UIContext::get_canvas(const std::string& name) {
    auto it = m_canvases.find(name);
    return it != m_canvases.end() ? it->second.get() : nullptr;
}

void UIContext::update(float dt, const UIInputState& input) {
    if (!m_initialized) return;

    // Reset cursor
    m_cursor = CursorType::Arrow;

    // Handle gamepad/keyboard navigation
    NavDirection nav_dir = input.get_nav_direction();
    if (nav_dir != NavDirection::None) {
        // Navigate focus on the topmost enabled canvas
        for (auto it = m_canvas_order.rbegin(); it != m_canvas_order.rend(); ++it) {
            if ((*it)->is_enabled()) {
                (*it)->navigate_focus(nav_dir);
                break;
            }
        }
    }

    // Handle confirm button
    if (input.was_confirm_pressed()) {
        for (auto it = m_canvas_order.rbegin(); it != m_canvas_order.rend(); ++it) {
            if ((*it)->is_enabled() && (*it)->get_focused_element()) {
                (*it)->activate_focused();
                break;
            }
        }
    }

    // Handle tab navigation
    if (input.key_tab) {
        for (auto it = m_canvas_order.rbegin(); it != m_canvas_order.rend(); ++it) {
            if ((*it)->is_enabled()) {
                (*it)->focus_next();
                break;
            }
        }
    }

    // Update all canvases
    for (auto* canvas : m_canvas_order) {
        canvas->update(dt, input);
    }

    // Update tooltip
    update_tooltip(dt, input);
}

void UIContext::update_tooltip(float dt, const UIInputState& input) {
    // Find element under mouse
    UIElement* hovered = nullptr;
    for (auto it = m_canvas_order.rbegin(); it != m_canvas_order.rend(); ++it) {
        UICanvas* canvas = *it;
        if (canvas->is_enabled()) {
            hovered = canvas->find_element_at(input.mouse_position);
            if (hovered) break;
        }
    }

    // Check if hovered element has tooltip
    UIElement* tooltip_source = nullptr;
    if (hovered && hovered->has_tooltip()) {
        tooltip_source = hovered;
    }

    // Update tooltip state
    if (tooltip_source != m_tooltip_element) {
        m_tooltip_element = tooltip_source;
        m_tooltip_timer = 0.0f;
        m_tooltip_visible = false;
        m_tooltip_position = input.mouse_position;
    } else if (m_tooltip_element) {
        m_tooltip_timer += dt;
        if (m_tooltip_timer >= m_tooltip_delay && !m_tooltip_visible) {
            m_tooltip_visible = true;
            // Position tooltip below and to the right of cursor
            m_tooltip_position = input.mouse_position + Vec2(12.0f, 16.0f);
        }
    }
}

void UIContext::render(render::RenderView view) {
    if (!m_initialized) return;

    // Begin render context
    m_render_context.begin(m_screen_width, m_screen_height);

    // Render all canvases
    for (auto* canvas : m_canvas_order) {
        if (canvas->is_enabled()) {
            canvas->render(m_render_context);
        }
    }

    // Render tooltip on top
    render_tooltip();

    // End render context
    m_render_context.end();

    // Submit to renderer
    m_renderer.render(m_render_context, view);
}

void UIContext::render_tooltip() {
    if (!m_tooltip_visible || !m_tooltip_element) return;

    const std::string& tooltip_text = m_tooltip_element->get_tooltip();
    if (tooltip_text.empty()) return;

    // Measure text
    FontHandle font = m_font_manager.get_default_font();
    float font_size = 14.0f;
    Vec2 text_size = m_font_manager.measure_text(font, tooltip_text);

    // Calculate tooltip bounds with padding
    float padding = 6.0f;
    Vec2 tooltip_size(text_size.x + padding * 2, text_size.y + padding * 2);

    // Clamp position to screen bounds
    Vec2 pos = m_tooltip_position;
    if (pos.x + tooltip_size.x > m_screen_width) {
        pos.x = m_screen_width - tooltip_size.x;
    }
    if (pos.y + tooltip_size.y > m_screen_height) {
        pos.y = m_tooltip_position.y - tooltip_size.y - 8.0f;
    }

    Rect tooltip_rect(pos.x, pos.y, tooltip_size.x, tooltip_size.y);

    // Draw background
    m_render_context.draw_rect_rounded(tooltip_rect, Vec4(0.1f, 0.1f, 0.1f, 0.95f), 4.0f);
    m_render_context.draw_rect_outline_rounded(tooltip_rect, Vec4(0.3f, 0.3f, 0.3f, 1.0f), 1.0f, 4.0f);

    // Draw text
    Vec2 text_pos(tooltip_rect.x + padding, tooltip_rect.y + padding + text_size.y * 0.5f);
    m_render_context.draw_text(tooltip_text, text_pos, font, font_size,
                                Vec4(0.95f, 0.95f, 0.95f, 1.0f), HAlign::Left);
}

FontHandle UIContext::load_font(const std::string& path, float size_pixels) {
    return m_font_manager.load_font(path, size_pixels);
}

void UIContext::set_screen_size(uint32_t width, uint32_t height) {
    if (m_screen_width == width && m_screen_height == height) {
        return;
    }

    m_screen_width = width;
    m_screen_height = height;

    // Update all canvases
    for (auto& [name, canvas] : m_canvases) {
        canvas->set_size(width, height);
    }
}

void UIContext::sort_canvases() {
    std::sort(m_canvas_order.begin(), m_canvas_order.end(),
        [](const UICanvas* a, const UICanvas* b) {
            return a->get_sort_order() < b->get_sort_order();
        });
}

} // namespace engine::ui
