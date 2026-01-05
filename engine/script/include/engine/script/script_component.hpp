#pragma once

#include <engine/scene/entity.hpp>
#include <engine/scene/world.hpp>
#include <engine/core/log.hpp>
#include <sol/sol.hpp>
#include <string>
#include <unordered_map>

namespace engine::script {

// Register/unregister a World for hot reload support
void register_script_world(scene::World* world);
void unregister_script_world(scene::World* world);

// Script component that runs Lua code attached to an entity
struct ScriptComponent {
    std::string script_path;           // Path to the Lua script file
    sol::table instance;               // Lua table instance for this entity
    bool loaded = false;               // Whether script has been loaded
    bool enabled = true;               // Whether script is active

    // Exposed properties (editable, passed to Lua)
    std::unordered_map<std::string, sol::object> properties;

    ScriptComponent() = default;
    explicit ScriptComponent(const std::string& path) : script_path(path) {}
};

// Script system lifecycle functions
void script_system_init();
void script_system_shutdown();

// Update all scripts (call on_update for each)
void script_system_update(scene::World& world, float dt);

// Fixed update for physics-related scripts
void script_system_fixed_update(scene::World& world, float dt);

// Late update after all other updates
void script_system_late_update(scene::World& world, float dt);

// Load a script for an entity (called automatically if not loaded)
bool script_load(scene::World& world, scene::Entity entity);

// Unload a script from an entity
void script_unload(scene::World& world, scene::Entity entity);

// Reload a specific script file (hot reload)
void script_reload(const std::string& path);

// Reload all scripts
void script_reload_all();

// Call a method on an entity's script
template<typename... Args>
void script_call(scene::World& world, scene::Entity entity, const std::string& method, Args&&... args) {
    if (!world.has<ScriptComponent>(entity)) {
        return;
    }

    auto& script = world.get<ScriptComponent>(entity);
    if (!script.loaded || !script.instance.valid()) {
        return;
    }

    sol::function fn = script.instance[method];
    if (fn.valid()) {
        sol::protected_function_result result = fn(script.instance, std::forward<Args>(args)...);
        if (!result.valid()) {
            sol::error err = result;
            core::log(core::LogLevel::Error, "Script {} error: {}", method, err.what());
        }
    }
}

// Set a property on an entity's script
void script_set_property(scene::World& world, scene::Entity entity,
                         const std::string& name, sol::object value);

// Get a property from an entity's script
sol::object script_get_property(scene::World& world, scene::Entity entity,
                                const std::string& name);

} // namespace engine::script
