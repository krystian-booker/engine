#pragma once

#include <engine/core/math.hpp>
#include <string>
#include <cstdint>

namespace engine::core {

enum class Key : uint16_t {
    Unknown = 0,

    // Letters
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    // Numbers
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

    // Function keys
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    // Special keys
    Escape, Tab, CapsLock, Shift, Control, Alt, Space,
    Enter, Backspace, Delete, Insert, Home, End, PageUp, PageDown,

    // Arrow keys
    Up, Down, Left, Right,

    // Punctuation
    Comma, Period, Slash, Semicolon, Quote, LeftBracket, RightBracket,
    Backslash, Minus, Equals, Grave,

    // Numpad
    Numpad0, Numpad1, Numpad2, Numpad3, Numpad4,
    Numpad5, Numpad6, Numpad7, Numpad8, Numpad9,
    NumpadAdd, NumpadSubtract, NumpadMultiply, NumpadDivide,
    NumpadEnter, NumpadDecimal,

    // Modifiers
    LeftShift, RightShift, LeftControl, RightControl, LeftAlt, RightAlt,

    Count
};

enum class MouseButton : uint8_t {
    Left = 0,
    Right,
    Middle,
    Button4,
    Button5,
    Count
};

enum class GamepadButton : uint8_t {
    A = 0,
    B,
    X,
    Y,
    LeftBumper,
    RightBumper,
    Back,
    Start,
    Guide,
    LeftStick,
    RightStick,
    DpadUp,
    DpadRight,
    DpadDown,
    DpadLeft,
    Count
};

// Haptic feedback presets for common game events
enum class HapticPreset : uint8_t {
    None = 0,
    LightImpact,      // Light hit, footstep
    MediumImpact,     // Standard hit
    HeavyImpact,      // Heavy attack, explosion nearby
    Explosion,        // Large explosion
    Damage,           // Player takes damage
    CriticalDamage,   // Player takes heavy damage
    Footstep,         // Walking footstep
    Landing,          // Landing from jump/fall
    PickupItem,       // Collecting item
    UIConfirm,        // UI selection
    UICancel,         // UI cancel/back
    EngineRumble,     // Continuous vehicle engine
    Gunfire,          // Weapon fire
    Heartbeat,        // Low health heartbeat
};

class Input {
public:
    static void init();
    static void shutdown();
    static void update();  // Call at start of each frame

    // Keyboard
    static bool key_down(Key k);
    static bool key_pressed(Key k);   // True only on the frame the key was pressed
    static bool key_released(Key k);  // True only on the frame the key was released

    // Mouse
    static Vec2 mouse_pos();
    static Vec2 mouse_delta();
    static float scroll_delta();
    static bool mouse_down(MouseButton b);
    static bool mouse_pressed(MouseButton b);
    static bool mouse_released(MouseButton b);

    // Mouse capture (for FPS-style camera control)
    static void set_mouse_captured(bool captured);
    static bool is_mouse_captured();

    // Gamepad
    static bool gamepad_connected(int index = 0);
    static bool gamepad_button_down(int index, GamepadButton b);
    static bool gamepad_button_pressed(int index, GamepadButton b);
    static bool gamepad_button_released(int index, GamepadButton b);
    static Vec2 gamepad_left_stick(int index = 0);
    static Vec2 gamepad_right_stick(int index = 0);
    static float gamepad_left_trigger(int index = 0);
    static float gamepad_right_trigger(int index = 0);

    // Action mapping (for input abstraction)
    static void bind(const std::string& action, Key k);
    static void bind(const std::string& action, MouseButton b);
    static void bind(const std::string& action, GamepadButton b, int gamepad_index = 0);
    static void unbind(const std::string& action);
    static bool action_pressed(const std::string& action);
    static bool action_down(const std::string& action);
    static bool action_released(const std::string& action);
    static float action_value(const std::string& action);  // For analog inputs (0-1)

    // Haptic feedback (controller vibration)
    // Motor values are 0.0 to 1.0 intensity
    static void set_vibration(int gamepad_index, float left_motor, float right_motor);
    static void set_vibration_timed(int gamepad_index, float left_motor, float right_motor, float duration_seconds);
    static void stop_vibration(int gamepad_index);
    static void stop_all_vibration();

    // Haptic presets for common game feel
    static void play_haptic(int gamepad_index, HapticPreset preset, float intensity = 1.0f);

    // Update timed vibrations (called internally by update())
    static void update_haptics(float dt);

    // Input sensitivity/deadzone settings
    static void set_mouse_sensitivity(float sensitivity);
    static float get_mouse_sensitivity();
    static void set_invert_mouse_y(bool invert);
    static bool get_invert_mouse_y();
    static void set_gamepad_sensitivity(float sensitivity);
    static float get_gamepad_sensitivity();
    static void set_gamepad_deadzone(float deadzone);
    static float get_gamepad_deadzone();
    static void set_aim_assist_enabled(bool enabled);
    static bool get_aim_assist_enabled();
    static void set_aim_assist_strength(float strength);
    static float get_aim_assist_strength();

    // Called by platform layer to update input state
    static void on_key_event(Key k, bool pressed);
    static void on_mouse_button_event(MouseButton b, bool pressed);
    static void on_mouse_move_event(float x, float y);
    static void on_mouse_scroll_event(float delta);
    static void on_gamepad_button_event(int index, GamepadButton b, bool pressed);
    static void on_gamepad_axis_event(int index, int axis, float value);
};

} // namespace engine::core
