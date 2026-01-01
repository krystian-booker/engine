#include <engine/script/script_component.hpp>
#include <engine/script/lua_state.hpp>
#include <engine/scene/world.hpp>
#include <engine/core/log.hpp>
#include <unordered_set>

namespace engine::script {

void script_system_init() {
    init_lua();
}

void script_system_shutdown() {
    shutdown_lua();
}

bool script_load(scene::World& world, scene::Entity entity) {
    if (!world.has<ScriptComponent>(entity)) {
        return false;
    }

    auto& script = world.get<ScriptComponent>(entity);
    if (script.loaded || script.script_path.empty()) {
        return script.loaded;
    }

    auto& lua = get_lua();

    // Load the script file (should return a table/class definition)
    sol::object script_class = lua.load_script(script.script_path);
    if (!script_class.valid() || !script_class.is<sol::table>()) {
        core::log(core::LogLevel::Error, "Script '{}' did not return a valid table", script.script_path);
        return false;
    }

    // Create an instance of the script (shallow copy the table)
    sol::table class_table = script_class.as<sol::table>();
    sol::table instance = lua.create_table();

    // Copy methods and default values from class table
    for (auto& [key, value] : class_table) {
        instance[key] = value;
    }

    // Set the entity reference
    instance["entity"] = static_cast<uint32_t>(entity);

    // Apply any preset properties
    for (auto& [name, value] : script.properties) {
        instance[name] = value;
    }

    script.instance = instance;
    script.loaded = true;

    // Call on_create if it exists
    sol::function on_create = instance["on_create"];
    if (on_create.valid()) {
        sol::protected_function_result result = on_create(instance);
        if (!result.valid()) {
            sol::error err = result;
            core::log(core::LogLevel::Error, "Script on_create error: {}", err.what());
        }
    }

    return true;
}

void script_unload(scene::World& world, scene::Entity entity) {
    if (!world.has<ScriptComponent>(entity)) {
        return;
    }

    auto& script = world.get<ScriptComponent>(entity);
    if (!script.loaded) {
        return;
    }

    // Call on_destroy if it exists
    if (script.instance.valid()) {
        sol::function on_destroy = script.instance["on_destroy"];
        if (on_destroy.valid()) {
            sol::protected_function_result result = on_destroy(script.instance);
            if (!result.valid()) {
                sol::error err = result;
                core::log(core::LogLevel::Error, "Script on_destroy error: {}", err.what());
            }
        }
    }

    script.instance = sol::nil;
    script.loaded = false;
}

void script_system_update(scene::World& world, float dt) {
    auto view = world.view<ScriptComponent>();

    for (auto entity : view) {
        auto& script = view.get<ScriptComponent>(entity);

        if (!script.enabled) continue;

        // Lazy load
        if (!script.loaded) {
            script_load(world, entity);
        }

        if (!script.loaded || !script.instance.valid()) continue;

        // Call on_update
        sol::function on_update = script.instance["on_update"];
        if (on_update.valid()) {
            sol::protected_function_result result = on_update(script.instance, dt);
            if (!result.valid()) {
                sol::error err = result;
                core::log(core::LogLevel::Error, "Script on_update error: {}", err.what());
            }
        }
    }
}

void script_system_fixed_update(scene::World& world, float dt) {
    auto view = world.view<ScriptComponent>();

    for (auto entity : view) {
        auto& script = view.get<ScriptComponent>(entity);

        if (!script.enabled || !script.loaded || !script.instance.valid()) continue;

        // Call on_fixed_update
        sol::function on_fixed_update = script.instance["on_fixed_update"];
        if (on_fixed_update.valid()) {
            sol::protected_function_result result = on_fixed_update(script.instance, dt);
            if (!result.valid()) {
                sol::error err = result;
                core::log(core::LogLevel::Error, "Script on_fixed_update error: {}", err.what());
            }
        }
    }
}

void script_system_late_update(scene::World& world, float dt) {
    auto view = world.view<ScriptComponent>();

    for (auto entity : view) {
        auto& script = view.get<ScriptComponent>(entity);

        if (!script.enabled || !script.loaded || !script.instance.valid()) continue;

        // Call on_late_update
        sol::function on_late_update = script.instance["on_late_update"];
        if (on_late_update.valid()) {
            sol::protected_function_result result = on_late_update(script.instance, dt);
            if (!result.valid()) {
                sol::error err = result;
                core::log(core::LogLevel::Error, "Script on_late_update error: {}", err.what());
            }
        }
    }
}

void script_reload(const std::string& path) {
    core::log(core::LogLevel::Info, "Reloading script: {}", path);

    // Note: This would be called from the hot reload system
    // We need access to the world to iterate entities, which would typically
    // be provided via an event or callback system

    // For now, this is a placeholder - the actual implementation would
    // iterate all worlds and reload scripts that match the path
}

void script_reload_all() {
    core::log(core::LogLevel::Info, "Reloading all scripts");
    // Similar to above - would require world access
}

void script_set_property(scene::World& world, scene::Entity entity,
                         const std::string& name, sol::object value) {
    if (!world.has<ScriptComponent>(entity)) {
        return;
    }

    auto& script = world.get<ScriptComponent>(entity);
    script.properties[name] = value;

    if (script.loaded && script.instance.valid()) {
        script.instance[name] = value;
    }
}

sol::object script_get_property(scene::World& world, scene::Entity entity,
                                const std::string& name) {
    if (!world.has<ScriptComponent>(entity)) {
        return sol::nil;
    }

    auto& script = world.get<ScriptComponent>(entity);

    if (script.loaded && script.instance.valid()) {
        return script.instance[name];
    }

    auto it = script.properties.find(name);
    if (it != script.properties.end()) {
        return it->second;
    }

    return sol::nil;
}

} // namespace engine::script
