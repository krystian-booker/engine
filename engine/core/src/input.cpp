#include <engine/core/input.hpp>
#include <unordered_map>
#include <array>
#include <variant>

namespace engine::core {

namespace {

struct InputState {
    // Keyboard state
    std::array<bool, static_cast<size_t>(Key::Count)> keys_current{};
    std::array<bool, static_cast<size_t>(Key::Count)> keys_previous{};

    // Mouse state
    std::array<bool, static_cast<size_t>(MouseButton::Count)> mouse_current{};
    std::array<bool, static_cast<size_t>(MouseButton::Count)> mouse_previous{};
    Vec2 mouse_pos{0.0f};
    Vec2 mouse_pos_previous{0.0f};
    float scroll_delta = 0.0f;
    bool mouse_captured = false;

    // Gamepad state
    static constexpr int MAX_GAMEPADS = 4;
    std::array<bool, MAX_GAMEPADS> gamepad_connected{};
    std::array<std::array<bool, static_cast<size_t>(GamepadButton::Count)>, MAX_GAMEPADS> gamepad_buttons_current{};
    std::array<std::array<bool, static_cast<size_t>(GamepadButton::Count)>, MAX_GAMEPADS> gamepad_buttons_previous{};
    std::array<Vec2, MAX_GAMEPADS> left_sticks{};
    std::array<Vec2, MAX_GAMEPADS> right_sticks{};
    std::array<float, MAX_GAMEPADS> left_triggers{};
    std::array<float, MAX_GAMEPADS> right_triggers{};

    // Action bindings
    struct Binding {
        std::variant<Key, MouseButton, std::pair<GamepadButton, int>> input;
    };
    std::unordered_map<std::string, std::vector<Binding>> bindings;

    bool initialized = false;
};

InputState g_input;

} // anonymous namespace

void Input::init() {
    g_input = InputState{};
    g_input.initialized = true;
}

void Input::shutdown() {
    g_input.initialized = false;
}

void Input::update() {
    // Store previous states
    g_input.keys_previous = g_input.keys_current;
    g_input.mouse_previous = g_input.mouse_current;
    g_input.mouse_pos_previous = g_input.mouse_pos;
    g_input.scroll_delta = 0.0f;

    for (int i = 0; i < InputState::MAX_GAMEPADS; ++i) {
        g_input.gamepad_buttons_previous[i] = g_input.gamepad_buttons_current[i];
    }
}

// Keyboard
bool Input::key_down(Key k) {
    return g_input.keys_current[static_cast<size_t>(k)];
}

bool Input::key_pressed(Key k) {
    size_t idx = static_cast<size_t>(k);
    return g_input.keys_current[idx] && !g_input.keys_previous[idx];
}

bool Input::key_released(Key k) {
    size_t idx = static_cast<size_t>(k);
    return !g_input.keys_current[idx] && g_input.keys_previous[idx];
}

// Mouse
Vec2 Input::mouse_pos() {
    return g_input.mouse_pos;
}

Vec2 Input::mouse_delta() {
    return g_input.mouse_pos - g_input.mouse_pos_previous;
}

float Input::scroll_delta() {
    return g_input.scroll_delta;
}

bool Input::mouse_down(MouseButton b) {
    return g_input.mouse_current[static_cast<size_t>(b)];
}

bool Input::mouse_pressed(MouseButton b) {
    size_t idx = static_cast<size_t>(b);
    return g_input.mouse_current[idx] && !g_input.mouse_previous[idx];
}

bool Input::mouse_released(MouseButton b) {
    size_t idx = static_cast<size_t>(b);
    return !g_input.mouse_current[idx] && g_input.mouse_previous[idx];
}

void Input::set_mouse_captured(bool captured) {
    g_input.mouse_captured = captured;
}

bool Input::is_mouse_captured() {
    return g_input.mouse_captured;
}

// Gamepad
bool Input::gamepad_connected(int index) {
    if (index < 0 || index >= InputState::MAX_GAMEPADS) return false;
    return g_input.gamepad_connected[index];
}

bool Input::gamepad_button_down(int index, GamepadButton b) {
    if (index < 0 || index >= InputState::MAX_GAMEPADS) return false;
    return g_input.gamepad_buttons_current[index][static_cast<size_t>(b)];
}

bool Input::gamepad_button_pressed(int index, GamepadButton b) {
    if (index < 0 || index >= InputState::MAX_GAMEPADS) return false;
    size_t idx = static_cast<size_t>(b);
    return g_input.gamepad_buttons_current[index][idx] && !g_input.gamepad_buttons_previous[index][idx];
}

bool Input::gamepad_button_released(int index, GamepadButton b) {
    if (index < 0 || index >= InputState::MAX_GAMEPADS) return false;
    size_t idx = static_cast<size_t>(b);
    return !g_input.gamepad_buttons_current[index][idx] && g_input.gamepad_buttons_previous[index][idx];
}

Vec2 Input::gamepad_left_stick(int index) {
    if (index < 0 || index >= InputState::MAX_GAMEPADS) return Vec2{0.0f};
    return g_input.left_sticks[index];
}

Vec2 Input::gamepad_right_stick(int index) {
    if (index < 0 || index >= InputState::MAX_GAMEPADS) return Vec2{0.0f};
    return g_input.right_sticks[index];
}

float Input::gamepad_left_trigger(int index) {
    if (index < 0 || index >= InputState::MAX_GAMEPADS) return 0.0f;
    return g_input.left_triggers[index];
}

float Input::gamepad_right_trigger(int index) {
    if (index < 0 || index >= InputState::MAX_GAMEPADS) return 0.0f;
    return g_input.right_triggers[index];
}

// Action mapping
void Input::bind(const std::string& action, Key k) {
    g_input.bindings[action].push_back({k});
}

void Input::bind(const std::string& action, MouseButton b) {
    g_input.bindings[action].push_back({b});
}

void Input::bind(const std::string& action, GamepadButton b, int gamepad_index) {
    g_input.bindings[action].push_back({std::make_pair(b, gamepad_index)});
}

void Input::unbind(const std::string& action) {
    g_input.bindings.erase(action);
}

bool Input::action_pressed(const std::string& action) {
    auto it = g_input.bindings.find(action);
    if (it == g_input.bindings.end()) return false;

    for (const auto& binding : it->second) {
        if (std::holds_alternative<Key>(binding.input)) {
            if (key_pressed(std::get<Key>(binding.input))) return true;
        } else if (std::holds_alternative<MouseButton>(binding.input)) {
            if (mouse_pressed(std::get<MouseButton>(binding.input))) return true;
        } else {
            auto [btn, idx] = std::get<std::pair<GamepadButton, int>>(binding.input);
            if (gamepad_button_pressed(idx, btn)) return true;
        }
    }
    return false;
}

bool Input::action_down(const std::string& action) {
    auto it = g_input.bindings.find(action);
    if (it == g_input.bindings.end()) return false;

    for (const auto& binding : it->second) {
        if (std::holds_alternative<Key>(binding.input)) {
            if (key_down(std::get<Key>(binding.input))) return true;
        } else if (std::holds_alternative<MouseButton>(binding.input)) {
            if (mouse_down(std::get<MouseButton>(binding.input))) return true;
        } else {
            auto [btn, idx] = std::get<std::pair<GamepadButton, int>>(binding.input);
            if (gamepad_button_down(idx, btn)) return true;
        }
    }
    return false;
}

bool Input::action_released(const std::string& action) {
    auto it = g_input.bindings.find(action);
    if (it == g_input.bindings.end()) return false;

    for (const auto& binding : it->second) {
        if (std::holds_alternative<Key>(binding.input)) {
            if (key_released(std::get<Key>(binding.input))) return true;
        } else if (std::holds_alternative<MouseButton>(binding.input)) {
            if (mouse_released(std::get<MouseButton>(binding.input))) return true;
        } else {
            auto [btn, idx] = std::get<std::pair<GamepadButton, int>>(binding.input);
            if (gamepad_button_released(idx, btn)) return true;
        }
    }
    return false;
}

float Input::action_value(const std::string& action) {
    return action_down(action) ? 1.0f : 0.0f;
}

// Platform event handlers
void Input::on_key_event(Key k, bool pressed) {
    g_input.keys_current[static_cast<size_t>(k)] = pressed;
}

void Input::on_mouse_button_event(MouseButton b, bool pressed) {
    g_input.mouse_current[static_cast<size_t>(b)] = pressed;
}

void Input::on_mouse_move_event(float x, float y) {
    g_input.mouse_pos = Vec2{x, y};
}

void Input::on_mouse_scroll_event(float delta) {
    g_input.scroll_delta = delta;
}

void Input::on_gamepad_button_event(int index, GamepadButton b, bool pressed) {
    if (index < 0 || index >= InputState::MAX_GAMEPADS) return;
    g_input.gamepad_buttons_current[index][static_cast<size_t>(b)] = pressed;
}

void Input::on_gamepad_axis_event(int index, int axis, float value) {
    if (index < 0 || index >= InputState::MAX_GAMEPADS) return;

    switch (axis) {
        case 0: g_input.left_sticks[index].x = value; break;
        case 1: g_input.left_sticks[index].y = value; break;
        case 2: g_input.right_sticks[index].x = value; break;
        case 3: g_input.right_sticks[index].y = value; break;
        case 4: g_input.left_triggers[index] = value; break;
        case 5: g_input.right_triggers[index] = value; break;
    }
}

} // namespace engine::core
