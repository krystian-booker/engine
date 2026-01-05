#include <engine/core/input.hpp>
#include <unordered_map>
#include <array>
#include <variant>
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xinput.h>
#pragma comment(lib, "xinput.lib")
#endif

namespace engine::core {

namespace {

// Haptic vibration state for timed vibrations
struct HapticState {
    float left_motor = 0.0f;
    float right_motor = 0.0f;
    float time_remaining = 0.0f;
    bool active = false;
};

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

    // Haptic feedback state
    std::array<HapticState, MAX_GAMEPADS> haptics{};

    // Action bindings
    struct Binding {
        std::variant<Key, MouseButton, std::pair<GamepadButton, int>> input;
    };
    std::unordered_map<std::string, std::vector<Binding>> bindings;

    bool initialized = false;
};

InputState g_input;

// Platform-specific vibration implementation
void apply_vibration_to_hardware(int index, float left, float right) {
#ifdef _WIN32
    if (index < 0 || index >= InputState::MAX_GAMEPADS) return;

    XINPUT_VIBRATION vibration;
    vibration.wLeftMotorSpeed = static_cast<WORD>(std::clamp(left, 0.0f, 1.0f) * 65535.0f);
    vibration.wRightMotorSpeed = static_cast<WORD>(std::clamp(right, 0.0f, 1.0f) * 65535.0f);
    XInputSetState(static_cast<DWORD>(index), &vibration);
#else
    // Non-Windows platforms: no-op for now
    (void)index;
    (void)left;
    (void)right;
#endif
}

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

// Haptic feedback implementation
void Input::set_vibration(int gamepad_index, float left_motor, float right_motor) {
    if (gamepad_index < 0 || gamepad_index >= InputState::MAX_GAMEPADS) return;

    // Cancel any timed vibration
    g_input.haptics[gamepad_index].active = false;
    g_input.haptics[gamepad_index].time_remaining = 0.0f;

    // Apply immediately
    apply_vibration_to_hardware(gamepad_index, left_motor, right_motor);
}

void Input::set_vibration_timed(int gamepad_index, float left_motor, float right_motor, float duration_seconds) {
    if (gamepad_index < 0 || gamepad_index >= InputState::MAX_GAMEPADS) return;

    auto& haptic = g_input.haptics[gamepad_index];
    haptic.left_motor = std::clamp(left_motor, 0.0f, 1.0f);
    haptic.right_motor = std::clamp(right_motor, 0.0f, 1.0f);
    haptic.time_remaining = duration_seconds;
    haptic.active = true;

    // Apply immediately
    apply_vibration_to_hardware(gamepad_index, haptic.left_motor, haptic.right_motor);
}

void Input::stop_vibration(int gamepad_index) {
    if (gamepad_index < 0 || gamepad_index >= InputState::MAX_GAMEPADS) return;

    g_input.haptics[gamepad_index].active = false;
    g_input.haptics[gamepad_index].time_remaining = 0.0f;
    apply_vibration_to_hardware(gamepad_index, 0.0f, 0.0f);
}

void Input::stop_all_vibration() {
    for (int i = 0; i < InputState::MAX_GAMEPADS; ++i) {
        stop_vibration(i);
    }
}

void Input::play_haptic(int gamepad_index, HapticPreset preset, float intensity) {
    if (gamepad_index < 0 || gamepad_index >= InputState::MAX_GAMEPADS) return;

    intensity = std::clamp(intensity, 0.0f, 1.0f);

    // Preset definitions: {left_motor, right_motor, duration}
    // Left motor = low frequency rumble (heavy), Right motor = high frequency (light)
    float left = 0.0f, right = 0.0f, duration = 0.0f;

    switch (preset) {
        case HapticPreset::None:
            stop_vibration(gamepad_index);
            return;

        case HapticPreset::LightImpact:
            left = 0.0f;
            right = 0.3f;
            duration = 0.08f;
            break;

        case HapticPreset::MediumImpact:
            left = 0.3f;
            right = 0.5f;
            duration = 0.12f;
            break;

        case HapticPreset::HeavyImpact:
            left = 0.7f;
            right = 0.4f;
            duration = 0.18f;
            break;

        case HapticPreset::Explosion:
            left = 1.0f;
            right = 0.8f;
            duration = 0.35f;
            break;

        case HapticPreset::Damage:
            left = 0.5f;
            right = 0.6f;
            duration = 0.15f;
            break;

        case HapticPreset::CriticalDamage:
            left = 0.8f;
            right = 0.7f;
            duration = 0.25f;
            break;

        case HapticPreset::Footstep:
            left = 0.1f;
            right = 0.0f;
            duration = 0.05f;
            break;

        case HapticPreset::Landing:
            left = 0.4f;
            right = 0.2f;
            duration = 0.1f;
            break;

        case HapticPreset::PickupItem:
            left = 0.0f;
            right = 0.2f;
            duration = 0.06f;
            break;

        case HapticPreset::UIConfirm:
            left = 0.0f;
            right = 0.15f;
            duration = 0.04f;
            break;

        case HapticPreset::UICancel:
            left = 0.1f;
            right = 0.1f;
            duration = 0.03f;
            break;

        case HapticPreset::EngineRumble:
            // Continuous - use set_vibration instead for ongoing effects
            left = 0.2f;
            right = 0.1f;
            duration = 0.1f;
            break;

        case HapticPreset::Gunfire:
            left = 0.4f;
            right = 0.7f;
            duration = 0.08f;
            break;

        case HapticPreset::Heartbeat:
            left = 0.6f;
            right = 0.0f;
            duration = 0.15f;
            break;
    }

    // Apply intensity scaling
    left *= intensity;
    right *= intensity;

    set_vibration_timed(gamepad_index, left, right, duration);
}

void Input::update_haptics(float dt) {
    for (int i = 0; i < InputState::MAX_GAMEPADS; ++i) {
        auto& haptic = g_input.haptics[i];
        if (haptic.active) {
            haptic.time_remaining -= dt;
            if (haptic.time_remaining <= 0.0f) {
                haptic.active = false;
                haptic.time_remaining = 0.0f;
                apply_vibration_to_hardware(i, 0.0f, 0.0f);
            }
        }
    }
}

} // namespace engine::core
