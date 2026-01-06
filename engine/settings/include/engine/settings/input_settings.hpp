#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace engine::settings {

// ============================================================================
// Key Codes (subset for bindings)
// ============================================================================

enum class KeyCode : uint16_t {
    None = 0,

    // Letters
    A = 'A', B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    // Numbers
    Num0 = '0', Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

    // Function keys
    F1 = 256, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    // Arrow keys
    Up, Down, Left, Right,

    // Modifiers
    LeftShift, RightShift,
    LeftCtrl, RightCtrl,
    LeftAlt, RightAlt,

    // Common keys
    Space, Enter, Tab, Escape,
    Backspace, Delete, Insert,
    Home, End, PageUp, PageDown,
    CapsLock, NumLock, ScrollLock,

    // Numpad
    NumPad0, NumPad1, NumPad2, NumPad3, NumPad4,
    NumPad5, NumPad6, NumPad7, NumPad8, NumPad9,
    NumPadAdd, NumPadSubtract, NumPadMultiply, NumPadDivide,
    NumPadDecimal, NumPadEnter,

    // Punctuation
    Comma, Period, Semicolon, Quote,
    LeftBracket, RightBracket, Backslash, Slash,
    Grave, Minus, Equals,

    // Mouse
    MouseLeft = 400, MouseRight, MouseMiddle,
    Mouse4, Mouse5, MouseWheelUp, MouseWheelDown,

    Count
};

// ============================================================================
// Gamepad Button
// ============================================================================

enum class GamepadButton : uint8_t {
    None = 0,

    // Face buttons
    A, B, X, Y,

    // Shoulder/triggers
    LeftBumper, RightBumper,
    LeftTrigger, RightTrigger,

    // Sticks
    LeftStick, RightStick,

    // D-Pad
    DPadUp, DPadDown, DPadLeft, DPadRight,

    // Menu buttons
    Start, Select, Guide,

    Count
};

// ============================================================================
// Gamepad Axis
// ============================================================================

enum class GamepadAxis : uint8_t {
    LeftStickX,
    LeftStickY,
    RightStickX,
    RightStickY,
    LeftTrigger,
    RightTrigger,
    Count
};

// ============================================================================
// Input Binding
// ============================================================================

struct InputBinding {
    std::string action;             // Action name: "move_forward", "attack", etc.
    KeyCode primary_key = KeyCode::None;
    KeyCode secondary_key = KeyCode::None;
    GamepadButton gamepad_button = GamepadButton::None;
    GamepadAxis gamepad_axis = GamepadAxis::LeftStickX;
    bool uses_axis = false;
    bool axis_inverted = false;
    float axis_deadzone = 0.15f;
};

// ============================================================================
// Input Settings
// ============================================================================

struct InputSettings {
    // ========================================================================
    // Mouse Settings
    // ========================================================================

    float mouse_sensitivity = 1.0f;
    float mouse_sensitivity_x = 1.0f;
    float mouse_sensitivity_y = 1.0f;
    bool invert_mouse_y = false;
    bool raw_mouse_input = true;
    float mouse_smoothing = 0.0f;   // 0 = none
    float mouse_acceleration = 0.0f;

    // ========================================================================
    // Gamepad Settings
    // ========================================================================

    float gamepad_sensitivity = 1.0f;
    float gamepad_sensitivity_x = 1.0f;
    float gamepad_sensitivity_y = 1.0f;
    bool invert_gamepad_y = false;
    float left_stick_deadzone = 0.15f;
    float right_stick_deadzone = 0.15f;
    float trigger_deadzone = 0.1f;

    // ========================================================================
    // Aim Assist (for gamepad)
    // ========================================================================

    bool aim_assist_enabled = true;
    float aim_assist_strength = 0.5f;
    float aim_slowdown_strength = 0.3f;
    float aim_magnetism_strength = 0.2f;

    // ========================================================================
    // Haptics/Vibration
    // ========================================================================

    bool vibration_enabled = true;
    float vibration_intensity = 1.0f;

    // ========================================================================
    // Keybindings
    // ========================================================================

    std::unordered_map<std::string, InputBinding> bindings;

    // ========================================================================
    // Input Behavior
    // ========================================================================

    bool hold_to_crouch = false;
    bool hold_to_sprint = true;
    bool toggle_aim = false;
    float double_tap_time = 0.3f;   // For double-tap actions

    // ========================================================================
    // Methods
    // ========================================================================

    void set_binding(const std::string& action, KeyCode key);
    void set_binding(const std::string& action, GamepadButton button);
    void set_binding_axis(const std::string& action, GamepadAxis axis, bool inverted = false);
    void clear_binding(const std::string& action);

    InputBinding* get_binding(const std::string& action);
    const InputBinding* get_binding(const std::string& action) const;

    std::vector<std::string> get_actions_for_key(KeyCode key) const;
    std::vector<std::string> get_actions_for_button(GamepadButton button) const;

    void reset_to_defaults();
    void validate();
    bool operator==(const InputSettings& other) const;
};

// ============================================================================
// Helper Functions
// ============================================================================

std::string get_key_name(KeyCode key);
std::string get_button_name(GamepadButton button);
std::string get_axis_name(GamepadAxis axis);
KeyCode key_from_name(const std::string& name);
GamepadButton button_from_name(const std::string& name);

} // namespace engine::settings
