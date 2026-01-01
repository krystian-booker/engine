#pragma once

#include <engine/render/render_target.hpp>
#include <bgfx/bgfx.h>
#include <cstdint>

namespace engine::debug_gui {

// Input event for ImGui
struct ImGuiInputEvent {
    enum class Type {
        MouseMove,
        MouseButton,
        MouseScroll,
        Key,
        Char
    };

    Type type;

    // Mouse data
    float mouse_x = 0;
    float mouse_y = 0;
    int button = 0;          // 0=left, 1=right, 2=middle
    bool button_down = false;
    float scroll_x = 0;
    float scroll_y = 0;

    // Keyboard data
    int key = 0;
    bool key_down = false;
    uint32_t character = 0;
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
};

// Forward declare ImGui types
struct ImDrawData;

// ImGui integration layer for bgfx
class ImGuiLayer {
public:
    ImGuiLayer();
    ~ImGuiLayer();

    // Non-copyable
    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;

    // Initialize with window info
    bool init(void* native_window_handle, uint32_t width, uint32_t height);
    void shutdown();

    // Resize handling
    void resize(uint32_t width, uint32_t height);

    // Frame lifecycle
    void begin_frame(float dt);
    void end_frame();

    // Render ImGui draw data to bgfx
    void render(render::RenderView view);

    // Process input events
    void process_input(const ImGuiInputEvent& event);

    // State queries
    bool wants_capture_mouse() const;
    bool wants_capture_keyboard() const;
    bool is_initialized() const { return m_initialized; }

    // Font loading
    void add_font(const char* path, float size_pixels);
    void build_fonts();

private:
    void setup_style();
    void create_font_texture();
    void destroy_font_texture();
    void render_draw_data(ImDrawData* draw_data, render::RenderView view);

    bool m_initialized = false;
    uint32_t m_width = 0;
    uint32_t m_height = 0;

    // bgfx resources
    bgfx::TextureHandle m_font_texture = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout m_vertex_layout;
    bgfx::ProgramHandle m_program = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle m_u_texture = BGFX_INVALID_HANDLE;
};

} // namespace engine::debug_gui
