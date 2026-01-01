#pragma once

// Umbrella header for engine::debug_gui module

#if !defined(ENGINE_DEBUG_GUI_DISABLED)

#include <engine/debug-gui/imgui_layer.hpp>
#include <engine/debug-gui/debug_window.hpp>
#include <engine/debug-gui/debug_gui_manager.hpp>
#include <engine/debug-gui/debug_console.hpp>
#include <engine/debug-gui/debug_profiler.hpp>
#include <engine/debug-gui/debug_entity_inspector.hpp>

#else

// Stub definitions when debug GUI is disabled
namespace engine::debug_gui {

class DebugGuiManager {
public:
    static DebugGuiManager& instance() {
        static DebugGuiManager s_instance;
        return s_instance;
    }

    bool init(void*, uint32_t, uint32_t) { return true; }
    void shutdown() {}
    void begin_frame(float) {}
    void end_frame() {}
    void render(render::RenderView) {}
    void resize(uint32_t, uint32_t) {}
    bool process_input(const ImGuiInputEvent&) { return false; }
    void process_keyboard() {}
    void toggle_visible() {}
    bool is_visible() const { return false; }
    void set_visible(bool) {}
    void set_world(scene::World*) {}
};

} // namespace engine::debug_gui

#endif
