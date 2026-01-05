#include <engine/script/script_component.hpp>
#include <engine/script/script_context.hpp>
#include <engine/script/lua_state.hpp>
#include <engine/scene/world.hpp>
#include <engine/physics/physics_world.hpp>
#include <engine/physics/rigid_body_component.hpp>
#include <engine/core/log.hpp>
#include <unordered_set>
#include <unordered_map>
#include <mutex>

namespace engine::script {

// Set of registered worlds for hot reload
static std::unordered_set<scene::World*> s_registered_worlds;

// Collision tracking for enter/exit detection
struct CollisionPair {
    uint32_t body_a;
    uint32_t body_b;

    bool operator==(const CollisionPair& other) const {
        return (body_a == other.body_a && body_b == other.body_b) ||
               (body_a == other.body_b && body_b == other.body_a);
    }
};

struct CollisionPairHash {
    size_t operator()(const CollisionPair& p) const {
        // Order-independent hash
        auto a = std::min(p.body_a, p.body_b);
        auto b = std::max(p.body_a, p.body_b);
        return std::hash<uint64_t>{}((static_cast<uint64_t>(a) << 32) | b);
    }
};

// Pending collision event for dispatch
struct PendingCollisionEvent {
    physics::PhysicsBodyId body_a;
    physics::PhysicsBodyId body_b;
    core::Vec3 contact_point;
    core::Vec3 contact_normal;
    bool is_start;
};

static std::mutex s_collision_mutex;
static std::vector<PendingCollisionEvent> s_pending_collisions;
static std::unordered_set<CollisionPair, CollisionPairHash> s_active_collisions;

void register_script_world(scene::World* world) {
    if (world) {
        s_registered_worlds.insert(world);
    }
}

void unregister_script_world(scene::World* world) {
    s_registered_worlds.erase(world);
}

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
    // Set world context for component operations in Lua
    set_current_script_world(&world);

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

    // Clear world context
    set_current_script_world(nullptr);
}

void script_system_fixed_update(scene::World& world, float dt) {
    // Set world context for component operations in Lua
    set_current_script_world(&world);

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

    // Clear world context
    set_current_script_world(nullptr);
}

void script_system_late_update(scene::World& world, float dt) {
    // Set world context for component operations in Lua
    set_current_script_world(&world);

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

    // Clear world context
    set_current_script_world(nullptr);
}

void script_reload(const std::string& path) {
    core::log(core::LogLevel::Info, "Reloading script: {}", path);

    for (auto* world : s_registered_worlds) {
        auto view = world->view<ScriptComponent>();
        for (auto entity : view) {
            auto& script = view.get<ScriptComponent>(entity);

            if (script.script_path == path && script.loaded) {
                // Preserve current property values from the running instance
                std::unordered_map<std::string, sol::object> saved_props;
                if (script.instance.valid()) {
                    for (const auto& [key, _] : script.properties) {
                        sol::object val = script.instance[key];
                        if (val.valid()) {
                            saved_props[key] = val;
                        }
                    }
                }

                // Unload the script (calls on_destroy)
                script_unload(*world, entity);

                // Restore saved property values
                script.properties = std::move(saved_props);

                // Reload the script (calls on_create)
                script_load(*world, entity);

                core::log(core::LogLevel::Debug, "Reloaded script instance for entity {}",
                          static_cast<uint32_t>(entity));
            }
        }
    }
}

void script_reload_all() {
    core::log(core::LogLevel::Info, "Reloading all scripts");

    for (auto* world : s_registered_worlds) {
        auto view = world->view<ScriptComponent>();
        for (auto entity : view) {
            auto& script = view.get<ScriptComponent>(entity);

            if (script.loaded) {
                // Preserve current property values from the running instance
                std::unordered_map<std::string, sol::object> saved_props;
                if (script.instance.valid()) {
                    for (const auto& [key, _] : script.properties) {
                        sol::object val = script.instance[key];
                        if (val.valid()) {
                            saved_props[key] = val;
                        }
                    }
                }

                // Unload the script (calls on_destroy)
                script_unload(*world, entity);

                // Restore saved property values
                script.properties = std::move(saved_props);

                // Reload the script (calls on_create)
                script_load(*world, entity);
            }
        }
    }
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

// Helper to find entity from PhysicsBodyId
static scene::Entity find_entity_by_body_id(scene::World& world, physics::PhysicsBodyId body_id) {
    auto view = world.view<physics::RigidBodyComponent>();
    for (auto entity : view) {
        auto& rb = view.get<physics::RigidBodyComponent>(entity);
        if (rb.body_id.id == body_id.id) {
            return entity;
        }
    }
    return entt::null;
}

// Collision callback that stores events for later dispatch
static void collision_callback(const physics::CollisionEvent& event) {
    std::lock_guard<std::mutex> lock(s_collision_mutex);

    PendingCollisionEvent pending;
    pending.body_a = event.body_a;
    pending.body_b = event.body_b;
    pending.contact_point = event.contact.position;
    pending.contact_normal = event.contact.normal;
    pending.is_start = event.is_start;

    s_pending_collisions.push_back(pending);

    // Track active collisions
    CollisionPair pair{event.body_a.id, event.body_b.id};
    if (event.is_start) {
        s_active_collisions.insert(pair);
    } else {
        s_active_collisions.erase(pair);
    }
}

void script_collision_init() {
    auto* physics_world = get_script_context().physics_world;
    if (!physics_world) {
        core::log(core::LogLevel::Warn, "script_collision_init: physics world not available");
        return;
    }

    physics_world->set_collision_callback(collision_callback);
    core::log(core::LogLevel::Debug, "Script collision system initialized");
}

void script_collision_shutdown() {
    auto* physics_world = get_script_context().physics_world;
    if (physics_world) {
        physics_world->set_collision_callback(nullptr);
    }

    std::lock_guard<std::mutex> lock(s_collision_mutex);
    s_pending_collisions.clear();
    s_active_collisions.clear();
}

void script_collision_dispatch(scene::World& world) {
    // Grab pending events
    std::vector<PendingCollisionEvent> events;
    {
        std::lock_guard<std::mutex> lock(s_collision_mutex);
        events = std::move(s_pending_collisions);
        s_pending_collisions.clear();
    }

    if (events.empty()) return;

    set_current_script_world(&world);

    for (const auto& event : events) {
        // Find entities for both bodies
        scene::Entity entity_a = find_entity_by_body_id(world, event.body_a);
        scene::Entity entity_b = find_entity_by_body_id(world, event.body_b);

        if (entity_a == entt::null && entity_b == entt::null) continue;

        // Check if either body is a sensor (trigger)
        bool is_trigger_a = false;
        bool is_trigger_b = false;

        if (entity_a != entt::null) {
            auto* rb = world.try_get<physics::RigidBodyComponent>(entity_a);
            if (rb) is_trigger_a = rb->is_sensor;
        }
        if (entity_b != entt::null) {
            auto* rb = world.try_get<physics::RigidBodyComponent>(entity_b);
            if (rb) is_trigger_b = rb->is_sensor;
        }

        bool is_trigger = is_trigger_a || is_trigger_b;

        // Dispatch to entity A's script
        if (entity_a != entt::null && world.has<ScriptComponent>(entity_a)) {
            auto& script = world.get<ScriptComponent>(entity_a);
            if (script.loaded && script.enabled && script.instance.valid()) {
                uint32_t other_entity = static_cast<uint32_t>(entity_b);

                if (is_trigger) {
                    // Trigger callbacks
                    const char* method = event.is_start ? "on_trigger_enter" : "on_trigger_exit";
                    sol::function fn = script.instance[method];
                    if (fn.valid()) {
                        sol::protected_function_result result = fn(script.instance, other_entity);
                        if (!result.valid()) {
                            sol::error err = result;
                            core::log(core::LogLevel::Error, "Script {} error: {}", method, err.what());
                        }
                    }
                } else {
                    // Collision callbacks
                    if (event.is_start) {
                        sol::function fn = script.instance["on_collision_enter"];
                        if (fn.valid()) {
                            sol::protected_function_result result = fn(script.instance, other_entity,
                                                                        event.contact_point, event.contact_normal);
                            if (!result.valid()) {
                                sol::error err = result;
                                core::log(core::LogLevel::Error, "Script on_collision_enter error: {}", err.what());
                            }
                        }
                    } else {
                        sol::function fn = script.instance["on_collision_exit"];
                        if (fn.valid()) {
                            sol::protected_function_result result = fn(script.instance, other_entity);
                            if (!result.valid()) {
                                sol::error err = result;
                                core::log(core::LogLevel::Error, "Script on_collision_exit error: {}", err.what());
                            }
                        }
                    }
                }
            }
        }

        // Dispatch to entity B's script
        if (entity_b != entt::null && world.has<ScriptComponent>(entity_b)) {
            auto& script = world.get<ScriptComponent>(entity_b);
            if (script.loaded && script.enabled && script.instance.valid()) {
                uint32_t other_entity = static_cast<uint32_t>(entity_a);
                // Flip normal for entity B's perspective
                core::Vec3 flipped_normal = -event.contact_normal;

                if (is_trigger) {
                    // Trigger callbacks
                    const char* method = event.is_start ? "on_trigger_enter" : "on_trigger_exit";
                    sol::function fn = script.instance[method];
                    if (fn.valid()) {
                        sol::protected_function_result result = fn(script.instance, other_entity);
                        if (!result.valid()) {
                            sol::error err = result;
                            core::log(core::LogLevel::Error, "Script {} error: {}", method, err.what());
                        }
                    }
                } else {
                    // Collision callbacks
                    if (event.is_start) {
                        sol::function fn = script.instance["on_collision_enter"];
                        if (fn.valid()) {
                            sol::protected_function_result result = fn(script.instance, other_entity,
                                                                        event.contact_point, flipped_normal);
                            if (!result.valid()) {
                                sol::error err = result;
                                core::log(core::LogLevel::Error, "Script on_collision_enter error: {}", err.what());
                            }
                        }
                    } else {
                        sol::function fn = script.instance["on_collision_exit"];
                        if (fn.valid()) {
                            sol::protected_function_result result = fn(script.instance, other_entity);
                            if (!result.valid()) {
                                sol::error err = result;
                                core::log(core::LogLevel::Error, "Script on_collision_exit error: {}", err.what());
                            }
                        }
                    }
                }
            }
        }
    }

    set_current_script_world(nullptr);
}

} // namespace engine::script
