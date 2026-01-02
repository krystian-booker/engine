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

    // Update all canvases
    for (auto* canvas : m_canvas_order) {
        canvas->update(dt, input);
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

    // End render context
    m_render_context.end();

    // Submit to renderer
    m_renderer.render(m_render_context, view);
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
