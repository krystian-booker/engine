#include <engine/script/bindings.hpp>
#include <engine/core/input.hpp>

namespace engine::script {

void register_input_bindings(sol::state& lua) {
    using namespace engine::core;

    // Create input table
    auto input = lua.create_named_table("Input");

    // Keyboard
    input.set_function("is_key_down", [](int key_code) {
        return Input::key_down(static_cast<Key>(key_code));
    });

    input.set_function("is_key_pressed", [](int key_code) {
        return Input::key_pressed(static_cast<Key>(key_code));
    });

    input.set_function("is_key_released", [](int key_code) {
        return Input::key_released(static_cast<Key>(key_code));
    });

    // Mouse
    input.set_function("is_mouse_down", [](int button) {
        return Input::mouse_down(static_cast<MouseButton>(button));
    });

    input.set_function("is_mouse_pressed", [](int button) {
        return Input::mouse_pressed(static_cast<MouseButton>(button));
    });

    input.set_function("is_mouse_released", [](int button) {
        return Input::mouse_released(static_cast<MouseButton>(button));
    });

    input.set_function("mouse_position", []() {
        auto pos = Input::mouse_pos();
        return std::make_tuple(pos.x, pos.y);
    });

    input.set_function("mouse_delta", []() {
        auto delta = Input::mouse_delta();
        return std::make_tuple(delta.x, delta.y);
    });

    input.set_function("mouse_scroll", &Input::scroll_delta);

    input.set_function("set_mouse_captured", &Input::set_mouse_captured);
    input.set_function("is_mouse_captured", &Input::is_mouse_captured);

    // Gamepad
    input.set_function("is_gamepad_connected", [](int index) {
        return Input::gamepad_connected(index);
    });

    input.set_function("is_gamepad_button_down", [](int index, int button) {
        return Input::gamepad_button_down(index, static_cast<GamepadButton>(button));
    });

    input.set_function("is_gamepad_button_pressed", [](int index, int button) {
        return Input::gamepad_button_pressed(index, static_cast<GamepadButton>(button));
    });

    input.set_function("gamepad_left_stick", [](int index) {
        auto stick = Input::gamepad_left_stick(index);
        return std::make_tuple(stick.x, stick.y);
    });

    input.set_function("gamepad_right_stick", [](int index) {
        auto stick = Input::gamepad_right_stick(index);
        return std::make_tuple(stick.x, stick.y);
    });

    input.set_function("gamepad_left_trigger", [](int index) {
        return Input::gamepad_left_trigger(index);
    });

    input.set_function("gamepad_right_trigger", [](int index) {
        return Input::gamepad_right_trigger(index);
    });

    // Haptic feedback (controller vibration)
    input.set_function("set_vibration", [](int index, float left, float right) {
        Input::set_vibration(index, left, right);
    });

    input.set_function("set_vibration_timed", [](int index, float left, float right, float duration) {
        Input::set_vibration_timed(index, left, right, duration);
    });

    input.set_function("stop_vibration", [](int index) {
        Input::stop_vibration(index);
    });

    input.set_function("stop_all_vibration", []() {
        Input::stop_all_vibration();
    });

    input.set_function("play_haptic", [](int index, int preset, sol::optional<float> intensity) {
        float i = intensity.value_or(1.0f);
        Input::play_haptic(index, static_cast<HapticPreset>(preset), i);
    });

    // Action system
    input.set_function("bind_action", [](const std::string& action, int key_code) {
        Input::bind(action, static_cast<Key>(key_code));
    });

    input.set_function("is_action_down", [](const std::string& action) {
        return Input::action_down(action);
    });

    input.set_function("is_action_pressed", [](const std::string& action) {
        return Input::action_pressed(action);
    });

    input.set_function("is_action_released", [](const std::string& action) {
        return Input::action_released(action);
    });

    input.set_function("action_value", [](const std::string& action) {
        return Input::action_value(action);
    });

    // Mouse button constants
    input["MOUSE_LEFT"] = static_cast<int>(MouseButton::Left);
    input["MOUSE_RIGHT"] = static_cast<int>(MouseButton::Right);
    input["MOUSE_MIDDLE"] = static_cast<int>(MouseButton::Middle);

    // Gamepad button constants
    input["GAMEPAD_A"] = static_cast<int>(GamepadButton::A);
    input["GAMEPAD_B"] = static_cast<int>(GamepadButton::B);
    input["GAMEPAD_X"] = static_cast<int>(GamepadButton::X);
    input["GAMEPAD_Y"] = static_cast<int>(GamepadButton::Y);
    input["GAMEPAD_LB"] = static_cast<int>(GamepadButton::LeftBumper);
    input["GAMEPAD_RB"] = static_cast<int>(GamepadButton::RightBumper);
    input["GAMEPAD_BACK"] = static_cast<int>(GamepadButton::Back);
    input["GAMEPAD_START"] = static_cast<int>(GamepadButton::Start);
    input["GAMEPAD_LSTICK"] = static_cast<int>(GamepadButton::LeftStick);
    input["GAMEPAD_RSTICK"] = static_cast<int>(GamepadButton::RightStick);
    input["GAMEPAD_DPAD_UP"] = static_cast<int>(GamepadButton::DpadUp);
    input["GAMEPAD_DPAD_RIGHT"] = static_cast<int>(GamepadButton::DpadRight);
    input["GAMEPAD_DPAD_DOWN"] = static_cast<int>(GamepadButton::DpadDown);
    input["GAMEPAD_DPAD_LEFT"] = static_cast<int>(GamepadButton::DpadLeft);

    // Haptic preset constants
    input["HAPTIC_NONE"] = static_cast<int>(HapticPreset::None);
    input["HAPTIC_LIGHT_IMPACT"] = static_cast<int>(HapticPreset::LightImpact);
    input["HAPTIC_MEDIUM_IMPACT"] = static_cast<int>(HapticPreset::MediumImpact);
    input["HAPTIC_HEAVY_IMPACT"] = static_cast<int>(HapticPreset::HeavyImpact);
    input["HAPTIC_EXPLOSION"] = static_cast<int>(HapticPreset::Explosion);
    input["HAPTIC_DAMAGE"] = static_cast<int>(HapticPreset::Damage);
    input["HAPTIC_CRITICAL_DAMAGE"] = static_cast<int>(HapticPreset::CriticalDamage);
    input["HAPTIC_FOOTSTEP"] = static_cast<int>(HapticPreset::Footstep);
    input["HAPTIC_LANDING"] = static_cast<int>(HapticPreset::Landing);
    input["HAPTIC_PICKUP_ITEM"] = static_cast<int>(HapticPreset::PickupItem);
    input["HAPTIC_UI_CONFIRM"] = static_cast<int>(HapticPreset::UIConfirm);
    input["HAPTIC_UI_CANCEL"] = static_cast<int>(HapticPreset::UICancel);
    input["HAPTIC_ENGINE_RUMBLE"] = static_cast<int>(HapticPreset::EngineRumble);
    input["HAPTIC_GUNFIRE"] = static_cast<int>(HapticPreset::Gunfire);
    input["HAPTIC_HEARTBEAT"] = static_cast<int>(HapticPreset::Heartbeat);
}

} // namespace engine::script
