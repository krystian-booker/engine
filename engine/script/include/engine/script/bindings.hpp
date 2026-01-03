#pragma once

#include <sol/sol.hpp>

namespace engine::script {

class LuaState;

// Register all engine bindings with Lua
void register_all_bindings(LuaState& lua);

// Individual binding registrations
void register_math_bindings(sol::state& lua);
void register_entity_bindings(sol::state& lua);
void register_input_bindings(sol::state& lua);
void register_time_bindings(sol::state& lua);
void register_log_bindings(sol::state& lua);
void register_localization_bindings(sol::state& lua);

} // namespace engine::script
