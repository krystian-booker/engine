#include <engine/debug-gui/debug_gui_manager.hpp>
#include <engine/debug-gui/debug_console.hpp>
#include <engine/debug-gui/debug_profiler.hpp>
#include <engine/debug-gui/debug_entity_inspector.hpp>
#include <engine/core/input.hpp>
#include <engine/core/profiler.hpp>

#include <imgui.h>

namespace engine::debug_gui {

DebugGuiManager& DebugGuiManager::instance() {
    static DebugGuiManager s_instance;
    return s_instance;
}

bool DebugGuiManager::init(void* native_window_handle, uint32_t width, uint32_t height) {
    if (m_initialized) {
        return true;
    }

    if (!m_imgui_layer.init(native_window_handle, width, height)) {
        return false;
    }

    // Register built-in windows
    m_console_idx = m_windows.size();
    m_windows.push_back(std::make_unique<DebugConsole>());

    m_profiler_idx = m_windows.size();
    m_windows.push_back(std::make_unique<DebugProfiler>());

    m_entity_inspector_idx = m_windows.size();
    m_windows.push_back(std::make_unique<DebugEntityInspector>());

    m_initialized = true;
    return true;
}

void DebugGuiManager::shutdown() {
    if (!m_initialized) {
        return;
    }

    m_windows.clear();
    m_imgui_layer.shutdown();
    m_initialized = false;
}

void DebugGuiManager::begin_frame(float dt) {
    if (!m_initialized || !m_visible) return;
    m_imgui_layer.begin_frame(dt);
}

void DebugGuiManager::end_frame() {
    if (!m_initialized || !m_visible) return;

    draw_main_menu_bar();
    draw_windows();

    if (m_show_demo_window) {
        ImGui::ShowDemoWindow(&m_show_demo_window);
    }

    m_imgui_layer.end_frame();
}

void DebugGuiManager::render(render::RenderView view) {
    if (!m_initialized || !m_visible) return;
    m_imgui_layer.render(view);
}

void DebugGuiManager::resize(uint32_t width, uint32_t height) {
    if (!m_initialized) return;
    m_imgui_layer.resize(width, height);
}

bool DebugGuiManager::process_input(const ImGuiInputEvent& event) {
    if (!m_initialized || !m_visible) return false;

    m_imgui_layer.process_input(event);
    return m_imgui_layer.wants_capture_mouse() || m_imgui_layer.wants_capture_keyboard();
}

void DebugGuiManager::process_keyboard() {
    if (!m_initialized) return;

    // Toggle debug GUI with backtick key
    if (core::Input::key_pressed(core::Key::Grave)) {
        toggle_visible();
    }

    if (!m_visible) return;

    // Window shortcuts
    for (auto& window : m_windows) {
        uint32_t shortcut = window->get_shortcut_key();
        if (shortcut != 0 && core::Input::key_pressed(static_cast<core::Key>(shortcut))) {
            window->toggle();
        }
    }
}

void DebugGuiManager::toggle_visible() {
    m_visible = !m_visible;
}

void DebugGuiManager::register_window(std::unique_ptr<IDebugWindow> window) {
    m_windows.push_back(std::move(window));
}

DebugConsole* DebugGuiManager::get_console() {
    if (m_console_idx < m_windows.size()) {
        return static_cast<DebugConsole*>(m_windows[m_console_idx].get());
    }
    return nullptr;
}

DebugProfiler* DebugGuiManager::get_profiler() {
    if (m_profiler_idx < m_windows.size()) {
        return static_cast<DebugProfiler*>(m_windows[m_profiler_idx].get());
    }
    return nullptr;
}

DebugEntityInspector* DebugGuiManager::get_entity_inspector() {
    if (m_entity_inspector_idx < m_windows.size()) {
        return static_cast<DebugEntityInspector*>(m_windows[m_entity_inspector_idx].get());
    }
    return nullptr;
}

void DebugGuiManager::draw_main_menu_bar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Debug")) {
            for (auto& window : m_windows) {
                bool open = window->is_open();
                if (ImGui::MenuItem(window->get_title(), nullptr, &open)) {
                    window->set_open(open);
                }
            }
            ImGui::Separator();
            ImGui::MenuItem("ImGui Demo", nullptr, &m_show_demo_window);
            ImGui::Separator();
            if (ImGui::MenuItem("Hide Debug GUI", "`")) {
                m_visible = false;
            }
            ImGui::EndMenu();
        }

        // Show FPS on right side of menu bar
        const auto& stats = core::Profiler::get_frame_stats();
        char fps_text[64];
        snprintf(fps_text, sizeof(fps_text), "FPS: %d (%.1f ms)", stats.fps, stats.frame_time_ms);
        float text_width = ImGui::CalcTextSize(fps_text).x;
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - text_width - 10);
        ImGui::TextUnformatted(fps_text);

        ImGui::EndMainMenuBar();
    }
}

void DebugGuiManager::draw_windows() {
    // Pass world to entity inspector
    if (m_world && m_entity_inspector_idx < m_windows.size()) {
        auto* inspector = static_cast<DebugEntityInspector*>(m_windows[m_entity_inspector_idx].get());
        inspector->set_world(m_world);
    }

    // Draw all open windows
    for (auto& window : m_windows) {
        if (window->is_open()) {
            window->draw();
        }
    }
}

} // namespace engine::debug_gui
