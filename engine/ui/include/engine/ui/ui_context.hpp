#pragma once

#include <engine/ui/ui_canvas.hpp>
#include <engine/ui/ui_font.hpp>
#include <engine/ui/ui_renderer.hpp>
#include <engine/ui/ui_style.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine::render {
    class IRenderer;
}

namespace engine::ui {

// Main UI system context
class UIContext {
public:
    UIContext();
    ~UIContext();

    // Initialize/shutdown
    bool init(render::IRenderer* renderer);
    void shutdown();

    // Canvas management
    UICanvas* create_canvas(const std::string& name);
    void destroy_canvas(const std::string& name);
    UICanvas* get_canvas(const std::string& name);
    const std::vector<UICanvas*>& get_all_canvases() const { return m_canvas_order; }

    // Update all canvases
    void update(float dt, const UIInputState& input);

    // Render all canvases to their targets
    void render(render::RenderView view);

    // Font management
    FontHandle load_font(const std::string& path, float size_pixels);
    FontAtlas* get_font(FontHandle handle) { return m_font_manager.get_font(handle); }
    FontHandle get_default_font() const { return m_font_manager.get_default_font(); }
    void set_default_font(FontHandle font) { m_font_manager.set_default_font(font); }

    // Style sheet
    UIStyleSheet& style_sheet() { return m_style_sheet; }
    const UIStyleSheet& style_sheet() const { return m_style_sheet; }

    // Theme
    void set_theme(const UITheme& theme) { m_theme = theme; }
    const UITheme& get_theme() const { return m_theme; }

    // Screen size
    void set_screen_size(uint32_t width, uint32_t height);
    uint32_t get_screen_width() const { return m_screen_width; }
    uint32_t get_screen_height() const { return m_screen_height; }

    // DPI scaling
    void set_dpi_scale(float scale) { m_dpi_scale = scale; }
    float get_dpi_scale() const { return m_dpi_scale; }

    // Cursor
    enum class CursorType {
        Arrow,
        Text,
        Hand,
        ResizeH,
        ResizeV,
        ResizeDiag
    };
    void set_cursor(CursorType cursor) { m_cursor = cursor; }
    CursorType get_cursor() const { return m_cursor; }

    // Access subsystems
    FontManager& font_manager() { return m_font_manager; }
    UIRenderer& renderer() { return m_renderer; }

private:
    void sort_canvases();

    render::IRenderer* m_render = nullptr;

    std::unordered_map<std::string, std::unique_ptr<UICanvas>> m_canvases;
    std::vector<UICanvas*> m_canvas_order;  // Sorted by sort_order

    FontManager m_font_manager;
    UIRenderer m_renderer;
    UIRenderContext m_render_context;

    UIStyleSheet m_style_sheet;
    UITheme m_theme;

    uint32_t m_screen_width = 1920;
    uint32_t m_screen_height = 1080;
    float m_dpi_scale = 1.0f;

    CursorType m_cursor = CursorType::Arrow;

    bool m_initialized = false;
};

// Global UI context access (optional convenience)
UIContext* get_ui_context();
void set_ui_context(UIContext* ctx);

} // namespace engine::ui
