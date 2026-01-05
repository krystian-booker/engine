#include <engine/script/bindings.hpp>
#include <engine/script/lua_state.hpp>
#include <engine/core/log.hpp>
#include <engine/core/time.hpp>

namespace engine::script {

void register_time_bindings(sol::state& lua) {
    auto time = lua.create_named_table("Time");

    time.set_function("delta_time", &core::Time::delta_time);
    time.set_function("total_time", &core::Time::total_time);
    time.set_function("frame_count", &core::Time::frame_count);
}

void register_log_bindings(sol::state& lua) {
    auto log = lua.create_named_table("Log");

    log.set_function("trace", [](const std::string& msg) {
        core::log(core::LogLevel::Trace, "[Script] {}", msg);
    });
    log.set_function("debug", [](const std::string& msg) {
        core::log(core::LogLevel::Debug, "[Script] {}", msg);
    });
    log.set_function("info", [](const std::string& msg) {
        core::log(core::LogLevel::Info, "[Script] {}", msg);
    });
    log.set_function("warn", [](const std::string& msg) {
        core::log(core::LogLevel::Warn, "[Script] {}", msg);
    });
    log.set_function("error", [](const std::string& msg) {
        core::log(core::LogLevel::Error, "[Script] {}", msg);
    });
}

void register_all_bindings(LuaState& lua) {
    auto& state = lua.state();

    // Create main engine table
    auto engine = state.create_named_table("engine");

    // Register all subsystems
    register_math_bindings(state);
    register_entity_bindings(state);
    register_input_bindings(state);
    register_time_bindings(state);
    register_log_bindings(state);
    register_localization_bindings(state);
    register_physics_bindings(state);
    register_audio_bindings(state);
    register_navigation_bindings(state);
    register_debug_bindings(state);
    register_camera_bindings(state);
    register_animation_bindings(state);
    register_save_bindings(state);
    register_scene_bindings(state);
    register_ui_bindings(state);
    register_particle_bindings(state);
    register_render_bindings(state);
    register_cinematic_bindings(state);

    core::log(core::LogLevel::Debug, "Registered all Lua bindings");
}

} // namespace engine::script
