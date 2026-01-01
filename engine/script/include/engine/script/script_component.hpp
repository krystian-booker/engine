#pragma once

#include <engine/scene/entity.hpp>
#include <sol/sol.hpp>
#include <string>
#include <unordered_map>

namespace engine::scene {
class World;
}

namespace engine::script {

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
void script_call(scene::World& world, scene::Entity entity, const std::string& method, Args&&... args);

// Set a property on an entity's script
void script_set_property(scene::World& world, scene::Entity entity,
                         const std::string& name, sol::object value);

// Get a property from an entity's script
sol::object script_get_property(scene::World& world, scene::Entity entity,
                                const std::string& name);

} // namespace engine::script
