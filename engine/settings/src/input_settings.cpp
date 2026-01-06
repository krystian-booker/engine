#include <engine/settings/input_settings.hpp>
#include <algorithm>
#include <unordered_map>

namespace engine::settings {

// ============================================================================
// InputSettings
// ============================================================================

void InputSettings::set_binding(const std::string& action, KeyCode key) {
    if (bindings.contains(action)) {
        bindings[action].primary_key = key;
    } else {
        InputBinding binding;
        binding.action = action;
        binding.primary_key = key;
        bindings[action] = binding;
    }
}

void InputSettings::set_binding(const std::string& action, GamepadButton button) {
    if (bindings.contains(action)) {
        bindings[action].gamepad_button = button;
    } else {
        InputBinding binding;
        binding.action = action;
        binding.gamepad_button = button;
        bindings[action] = binding;
    }
}

void InputSettings::set_binding_axis(const std::string& action, GamepadAxis axis, bool inverted) {
    if (bindings.contains(action)) {
        bindings[action].gamepad_axis = axis;
        bindings[action].uses_axis = true;
        bindings[action].axis_inverted = inverted;
    } else {
        InputBinding binding;
        binding.action = action;
        binding.gamepad_axis = axis;
        binding.uses_axis = true;
        binding.axis_inverted = inverted;
        bindings[action] = binding;
    }
}

void InputSettings::clear_binding(const std::string& action) {
    bindings.erase(action);
}

InputBinding* InputSettings::get_binding(const std::string& action) {
    auto it = bindings.find(action);
    if (it != bindings.end()) {
        return &it->second;
    }
    return nullptr;
}

const InputBinding* InputSettings::get_binding(const std::string& action) const {
    auto it = bindings.find(action);
    if (it != bindings.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<std::string> InputSettings::get_actions_for_key(KeyCode key) const {
    std::vector<std::string> result;
    for (const auto& [action, binding] : bindings) {
        if (binding.primary_key == key || binding.secondary_key == key) {
            result.push_back(action);
        }
    }
    return result;
}

std::vector<std::string> InputSettings::get_actions_for_button(GamepadButton button) const {
    std::vector<std::string> result;
    for (const auto& [action, binding] : bindings) {
        if (binding.gamepad_button == button) {
            result.push_back(action);
        }
    }
    return result;
}

void InputSettings::reset_to_defaults() {
    bindings.clear();

    // Movement
    set_binding("move_forward", KeyCode::W);
    set_binding("move_backward", KeyCode::S);
    set_binding("move_left", KeyCode::A);
    set_binding("move_right", KeyCode::D);
    set_binding("jump", KeyCode::Space);
    set_binding("crouch", KeyCode::LeftCtrl);
    set_binding("sprint", KeyCode::LeftShift);
    set_binding("dodge", KeyCode::LeftAlt);

    // Combat
    set_binding("attack", KeyCode::MouseLeft);
    set_binding("heavy_attack", KeyCode::MouseRight);
    set_binding("block", KeyCode::MouseRight);
    set_binding("lock_target", KeyCode::Tab);
    set_binding("use_item", KeyCode::Q);

    // Interaction
    set_binding("interact", KeyCode::E);
    set_binding("inventory", KeyCode::I);
    set_binding("map", KeyCode::M);
    set_binding("pause", KeyCode::Escape);
    set_binding("quick_save", KeyCode::F5);
    set_binding("quick_load", KeyCode::F9);

    // Camera
    set_binding("camera_zoom_in", KeyCode::MouseWheelUp);
    set_binding("camera_zoom_out", KeyCode::MouseWheelDown);

    // Hotbar
    set_binding("hotbar_1", KeyCode::Num1);
    set_binding("hotbar_2", KeyCode::Num2);
    set_binding("hotbar_3", KeyCode::Num3);
    set_binding("hotbar_4", KeyCode::Num4);

    // Gamepad bindings
    set_binding("attack", GamepadButton::RightTrigger);
    set_binding("heavy_attack", GamepadButton::RightBumper);
    set_binding("block", GamepadButton::LeftTrigger);
    set_binding("dodge", GamepadButton::B);
    set_binding("jump", GamepadButton::A);
    set_binding("interact", GamepadButton::X);
    set_binding("use_item", GamepadButton::Y);
    set_binding("lock_target", GamepadButton::RightStick);
    set_binding("sprint", GamepadButton::LeftStick);
    set_binding("pause", GamepadButton::Start);
    set_binding("inventory", GamepadButton::Select);

    // Axis bindings
    set_binding_axis("move_x", GamepadAxis::LeftStickX);
    set_binding_axis("move_y", GamepadAxis::LeftStickY);
    set_binding_axis("look_x", GamepadAxis::RightStickX);
    set_binding_axis("look_y", GamepadAxis::RightStickY, true);  // Inverted by default
}

void InputSettings::validate() {
    mouse_sensitivity = std::clamp(mouse_sensitivity, 0.1f, 5.0f);
    mouse_sensitivity_x = std::clamp(mouse_sensitivity_x, 0.1f, 5.0f);
    mouse_sensitivity_y = std::clamp(mouse_sensitivity_y, 0.1f, 5.0f);
    mouse_smoothing = std::clamp(mouse_smoothing, 0.0f, 1.0f);
    mouse_acceleration = std::clamp(mouse_acceleration, 0.0f, 2.0f);

    gamepad_sensitivity = std::clamp(gamepad_sensitivity, 0.1f, 5.0f);
    gamepad_sensitivity_x = std::clamp(gamepad_sensitivity_x, 0.1f, 5.0f);
    gamepad_sensitivity_y = std::clamp(gamepad_sensitivity_y, 0.1f, 5.0f);
    left_stick_deadzone = std::clamp(left_stick_deadzone, 0.0f, 0.5f);
    right_stick_deadzone = std::clamp(right_stick_deadzone, 0.0f, 0.5f);
    trigger_deadzone = std::clamp(trigger_deadzone, 0.0f, 0.5f);

    aim_assist_strength = std::clamp(aim_assist_strength, 0.0f, 1.0f);
    aim_slowdown_strength = std::clamp(aim_slowdown_strength, 0.0f, 1.0f);
    aim_magnetism_strength = std::clamp(aim_magnetism_strength, 0.0f, 1.0f);

    vibration_intensity = std::clamp(vibration_intensity, 0.0f, 1.0f);
    double_tap_time = std::clamp(double_tap_time, 0.1f, 1.0f);
}

bool InputSettings::operator==(const InputSettings& other) const {
    if (mouse_sensitivity != other.mouse_sensitivity) return false;
    if (invert_mouse_y != other.invert_mouse_y) return false;
    if (gamepad_sensitivity != other.gamepad_sensitivity) return false;
    if (invert_gamepad_y != other.invert_gamepad_y) return false;
    if (vibration_enabled != other.vibration_enabled) return false;
    if (bindings.size() != other.bindings.size()) return false;
    // Full comparison would check all bindings
    return true;
}

// ============================================================================
// Helper Functions
// ============================================================================

std::string get_key_name(KeyCode key) {
    static const std::unordered_map<KeyCode, std::string> key_names = {
        {KeyCode::None, "None"},
        {KeyCode::A, "A"}, {KeyCode::B, "B"}, {KeyCode::C, "C"}, {KeyCode::D, "D"},
        {KeyCode::E, "E"}, {KeyCode::F, "F"}, {KeyCode::G, "G"}, {KeyCode::H, "H"},
        {KeyCode::I, "I"}, {KeyCode::J, "J"}, {KeyCode::K, "K"}, {KeyCode::L, "L"},
        {KeyCode::M, "M"}, {KeyCode::N, "N"}, {KeyCode::O, "O"}, {KeyCode::P, "P"},
        {KeyCode::Q, "Q"}, {KeyCode::R, "R"}, {KeyCode::S, "S"}, {KeyCode::T, "T"},
        {KeyCode::U, "U"}, {KeyCode::V, "V"}, {KeyCode::W, "W"}, {KeyCode::X, "X"},
        {KeyCode::Y, "Y"}, {KeyCode::Z, "Z"},
        {KeyCode::Num0, "0"}, {KeyCode::Num1, "1"}, {KeyCode::Num2, "2"},
        {KeyCode::Num3, "3"}, {KeyCode::Num4, "4"}, {KeyCode::Num5, "5"},
        {KeyCode::Num6, "6"}, {KeyCode::Num7, "7"}, {KeyCode::Num8, "8"}, {KeyCode::Num9, "9"},
        {KeyCode::F1, "F1"}, {KeyCode::F2, "F2"}, {KeyCode::F3, "F3"}, {KeyCode::F4, "F4"},
        {KeyCode::F5, "F5"}, {KeyCode::F6, "F6"}, {KeyCode::F7, "F7"}, {KeyCode::F8, "F8"},
        {KeyCode::F9, "F9"}, {KeyCode::F10, "F10"}, {KeyCode::F11, "F11"}, {KeyCode::F12, "F12"},
        {KeyCode::Up, "Up"}, {KeyCode::Down, "Down"}, {KeyCode::Left, "Left"}, {KeyCode::Right, "Right"},
        {KeyCode::LeftShift, "Left Shift"}, {KeyCode::RightShift, "Right Shift"},
        {KeyCode::LeftCtrl, "Left Ctrl"}, {KeyCode::RightCtrl, "Right Ctrl"},
        {KeyCode::LeftAlt, "Left Alt"}, {KeyCode::RightAlt, "Right Alt"},
        {KeyCode::Space, "Space"}, {KeyCode::Enter, "Enter"}, {KeyCode::Tab, "Tab"},
        {KeyCode::Escape, "Escape"}, {KeyCode::Backspace, "Backspace"},
        {KeyCode::Delete, "Delete"}, {KeyCode::Insert, "Insert"},
        {KeyCode::Home, "Home"}, {KeyCode::End, "End"},
        {KeyCode::PageUp, "Page Up"}, {KeyCode::PageDown, "Page Down"},
        {KeyCode::MouseLeft, "Mouse Left"}, {KeyCode::MouseRight, "Mouse Right"},
        {KeyCode::MouseMiddle, "Mouse Middle"}, {KeyCode::Mouse4, "Mouse 4"}, {KeyCode::Mouse5, "Mouse 5"},
        {KeyCode::MouseWheelUp, "Wheel Up"}, {KeyCode::MouseWheelDown, "Wheel Down"},
    };

    auto it = key_names.find(key);
    if (it != key_names.end()) {
        return it->second;
    }
    return "Unknown";
}

std::string get_button_name(GamepadButton button) {
    static const std::unordered_map<GamepadButton, std::string> button_names = {
        {GamepadButton::None, "None"},
        {GamepadButton::A, "A"}, {GamepadButton::B, "B"},
        {GamepadButton::X, "X"}, {GamepadButton::Y, "Y"},
        {GamepadButton::LeftBumper, "LB"}, {GamepadButton::RightBumper, "RB"},
        {GamepadButton::LeftTrigger, "LT"}, {GamepadButton::RightTrigger, "RT"},
        {GamepadButton::LeftStick, "LS"}, {GamepadButton::RightStick, "RS"},
        {GamepadButton::DPadUp, "D-Pad Up"}, {GamepadButton::DPadDown, "D-Pad Down"},
        {GamepadButton::DPadLeft, "D-Pad Left"}, {GamepadButton::DPadRight, "D-Pad Right"},
        {GamepadButton::Start, "Start"}, {GamepadButton::Select, "Select"},
        {GamepadButton::Guide, "Guide"},
    };

    auto it = button_names.find(button);
    if (it != button_names.end()) {
        return it->second;
    }
    return "Unknown";
}

std::string get_axis_name(GamepadAxis axis) {
    switch (axis) {
        case GamepadAxis::LeftStickX:  return "Left Stick X";
        case GamepadAxis::LeftStickY:  return "Left Stick Y";
        case GamepadAxis::RightStickX: return "Right Stick X";
        case GamepadAxis::RightStickY: return "Right Stick Y";
        case GamepadAxis::LeftTrigger: return "Left Trigger";
        case GamepadAxis::RightTrigger: return "Right Trigger";
        default: return "Unknown";
    }
}

KeyCode key_from_name(const std::string& name) {
    static const std::unordered_map<std::string, KeyCode> name_to_key = {
        {"A", KeyCode::A}, {"B", KeyCode::B}, {"C", KeyCode::C}, {"D", KeyCode::D},
        {"E", KeyCode::E}, {"F", KeyCode::F}, {"G", KeyCode::G}, {"H", KeyCode::H},
        {"I", KeyCode::I}, {"J", KeyCode::J}, {"K", KeyCode::K}, {"L", KeyCode::L},
        {"M", KeyCode::M}, {"N", KeyCode::N}, {"O", KeyCode::O}, {"P", KeyCode::P},
        {"Q", KeyCode::Q}, {"R", KeyCode::R}, {"S", KeyCode::S}, {"T", KeyCode::T},
        {"U", KeyCode::U}, {"V", KeyCode::V}, {"W", KeyCode::W}, {"X", KeyCode::X},
        {"Y", KeyCode::Y}, {"Z", KeyCode::Z},
        {"Space", KeyCode::Space}, {"Enter", KeyCode::Enter}, {"Tab", KeyCode::Tab},
        {"Escape", KeyCode::Escape}, {"Left Shift", KeyCode::LeftShift},
        {"Left Ctrl", KeyCode::LeftCtrl}, {"Left Alt", KeyCode::LeftAlt},
        {"Mouse Left", KeyCode::MouseLeft}, {"Mouse Right", KeyCode::MouseRight},
    };

    auto it = name_to_key.find(name);
    if (it != name_to_key.end()) {
        return it->second;
    }
    return KeyCode::None;
}

GamepadButton button_from_name(const std::string& name) {
    static const std::unordered_map<std::string, GamepadButton> name_to_button = {
        {"A", GamepadButton::A}, {"B", GamepadButton::B},
        {"X", GamepadButton::X}, {"Y", GamepadButton::Y},
        {"LB", GamepadButton::LeftBumper}, {"RB", GamepadButton::RightBumper},
        {"LT", GamepadButton::LeftTrigger}, {"RT", GamepadButton::RightTrigger},
        {"LS", GamepadButton::LeftStick}, {"RS", GamepadButton::RightStick},
        {"Start", GamepadButton::Start}, {"Select", GamepadButton::Select},
    };

    auto it = name_to_button.find(name);
    if (it != name_to_button.end()) {
        return it->second;
    }
    return GamepadButton::None;
}

} // namespace engine::settings
