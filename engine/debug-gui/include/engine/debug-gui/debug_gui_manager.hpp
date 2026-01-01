#pragma once

#include <engine/debug-gui/imgui_layer.hpp>
#include <engine/debug-gui/debug_window.hpp>
#include <vector>
#include <memory>

namespace engine::scene {
    class World;
}

namespace engine::debug_gui {

class DebugConsole;
class DebugProfiler;
class DebugEntityInspector;

// Central manager for debug GUI system
class DebugGuiManager {
public:
    static DebugGuiManager& instance();

    // Initialization
    bool init(void* native_window_handle, uint32_t width, uint32_t height);
    void shutdown();

    // Frame lifecycle
    void begin_frame(float dt);
    void end_frame();
    void render(render::RenderView view);

    // Resize handling
    void resize(uint32_t width, uint32_t height);

    // Input processing - returns true if input was consumed
    bool process_input(const ImGuiInputEvent& event);

    // Check keyboard for toggle key (call after Input::update())
    void process_keyboard();

    // Toggle entire debug GUI visibility
    void toggle_visible();
    bool is_visible() const { return m_visible; }
    void set_visible(bool visible) { m_visible = visible; }

    // Set world reference for entity inspector
    void set_world(scene::World* world) { m_world = world; }
    scene::World* get_world() const { return m_world; }

    // Register custom debug window
    void register_window(std::unique_ptr<IDebugWindow> window);

    // Access built-in windows
    DebugConsole* get_console();
    DebugProfiler* get_profiler();
    DebugEntityInspector* get_entity_inspector();

    // Get ImGui layer for advanced usage
    ImGuiLayer& get_imgui_layer() { return m_imgui_layer; }

private:
    DebugGuiManager() = default;
    ~DebugGuiManager() = default;
    DebugGuiManager(const DebugGuiManager&) = delete;
    DebugGuiManager& operator=(const DebugGuiManager&) = delete;

    void draw_main_menu_bar();
    void draw_windows();

    ImGuiLayer m_imgui_layer;
    std::vector<std::unique_ptr<IDebugWindow>> m_windows;
    scene::World* m_world = nullptr;
    bool m_visible = false;
    bool m_show_demo_window = false;
    bool m_initialized = false;

    // Indices to built-in windows
    size_t m_console_idx = 0;
    size_t m_profiler_idx = 0;
    size_t m_entity_inspector_idx = 0;
};

} // namespace engine::debug_gui
