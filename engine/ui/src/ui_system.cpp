#include <engine/ui/ui_system.hpp>

namespace engine::ui {

void ui_input_begin_frame(UIInputState& state) {
    // Store previous frame's state for detecting press/release
    for (int i = 0; i < 3; ++i) {
        state.prev_mouse_buttons[i] = state.mouse_buttons[i];
    }
    state.prev_nav_up = state.nav_up;
    state.prev_nav_down = state.nav_down;
    state.prev_nav_left = state.nav_left;
    state.prev_nav_right = state.nav_right;
    state.prev_nav_confirm = state.nav_confirm;
    state.prev_nav_cancel = state.nav_cancel;

    // Clear per-frame input
    state.text_input.clear();
    state.mouse_delta = Vec2(0.0f);
    state.scroll_delta = Vec2(0.0f);

    // Clear key states (will be set by events)
    state.key_backspace = false;
    state.key_delete = false;
    state.key_left = false;
    state.key_right = false;
    state.key_home = false;
    state.key_end = false;
    state.key_enter = false;
    state.key_tab = false;
    state.key_escape = false;
}

void ui_input_end_frame(UIInputState& state) {
    // Navigation states are typically set from gamepad/keyboard and persist
    // until explicitly cleared by the input system
    // Nothing to do here for now - state is ready for UI update
}

} // namespace engine::ui
