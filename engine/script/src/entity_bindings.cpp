#include <engine/script/bindings.hpp>
#include <engine/script/script_component.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <engine/scene/entity.hpp>
#include <engine/reflect/reflect.hpp>
#include <engine/reflect/type_registry.hpp>
#include <engine/core/log.hpp>

namespace engine::script {

void register_entity_bindings(sol::state& lua) {
    using namespace engine::scene;
    using namespace engine::core;

    // Entity as a simple integer handle
    lua.new_usertype<Entity>("Entity",
        sol::constructors<>(),
        sol::meta_function::to_string, [](Entity e) {
            return "Entity(" + std::to_string(static_cast<uint32_t>(e)) + ")";
        },
        sol::meta_function::equal_to, [](Entity a, Entity b) { return a == b; },
        "valid", [](Entity e) { return e != NullEntity; },
        "id", [](Entity e) { return static_cast<uint32_t>(e); }
    );

    // LocalTransform component
    lua.new_usertype<LocalTransform>("LocalTransform",
        sol::constructors<LocalTransform(), LocalTransform(const Vec3&)>(),
        "position", &LocalTransform::position,
        "rotation", &LocalTransform::rotation,
        "scale", &LocalTransform::scale,
        "forward", &LocalTransform::forward,
        "right", &LocalTransform::right,
        "up", &LocalTransform::up,
        "set_euler", &LocalTransform::set_euler,
        "euler", &LocalTransform::euler,
        "look_at", &LocalTransform::look_at,
        "matrix", &LocalTransform::matrix
    );

    // WorldTransform component (read-only computed transform)
    lua.new_usertype<WorldTransform>("WorldTransform",
        sol::constructors<>(),
        "matrix", sol::readonly(&WorldTransform::matrix),
        "position", &WorldTransform::position,
        "scale", &WorldTransform::scale,
        "rotation", &WorldTransform::rotation
    );

    // EntityInfo component
    lua.new_usertype<EntityInfo>("EntityInfo",
        sol::constructors<>(),
        "name", &EntityInfo::name,
        "uuid", sol::readonly(&EntityInfo::uuid),
        "enabled", &EntityInfo::enabled
    );

    // Engine namespace for entity operations
    auto engine = lua["engine"].get_or_create<sol::table>();

    // Check if entity has a component by type name
    engine.set_function("has_component", [](uint32_t entity_id, const std::string& type_name) -> bool {
        auto* world = get_current_script_world();
        if (!world) {
            core::log(core::LogLevel::Warn, "has_component called outside script context");
            return false;
        }

        auto& registry = world->registry();
        auto entity = static_cast<entt::entity>(entity_id);

        if (!registry.valid(entity)) {
            return false;
        }

        auto& type_reg = reflect::TypeRegistry::instance();
        auto component = type_reg.get_component_any(registry, entity, type_name);
        return component.operator bool();
    });

    // Get a component by type name - returns the component userdata or nil
    engine.set_function("get_component", [&lua](uint32_t entity_id, const std::string& type_name) -> sol::object {
        auto* world = get_current_script_world();
        if (!world) {
            core::log(core::LogLevel::Warn, "get_component called outside script context");
            return sol::nil;
        }

        auto& registry = world->registry();
        auto entity = static_cast<entt::entity>(entity_id);

        if (!registry.valid(entity)) {
            return sol::nil;
        }

        // Handle built-in types that have Lua bindings directly
        if (type_name == "LocalTransform") {
            if (registry.any_of<LocalTransform>(entity)) {
                return sol::make_object(lua, std::ref(registry.get<LocalTransform>(entity)));
            }
            return sol::nil;
        }
        if (type_name == "WorldTransform") {
            if (registry.any_of<WorldTransform>(entity)) {
                return sol::make_object(lua, std::ref(registry.get<WorldTransform>(entity)));
            }
            return sol::nil;
        }
        if (type_name == "EntityInfo") {
            if (registry.any_of<EntityInfo>(entity)) {
                return sol::make_object(lua, std::ref(registry.get<EntityInfo>(entity)));
            }
            return sol::nil;
        }

        // For other types, use reflection to check existence
        auto& type_reg = reflect::TypeRegistry::instance();
        auto component = type_reg.get_component_any(registry, entity, type_name);
        if (!component) {
            return sol::nil;
        }

        // For reflected types without direct Lua bindings, return a table representation
        // This is a simplified approach - a full implementation would expose properties
        const auto* type_info = type_reg.get_type_info(type_name);
        if (!type_info) {
            return sol::nil;
        }

        sol::table result = lua.create_table();
        result["_type"] = type_name;

        // Expose readable properties
        for (const auto& prop : type_info->properties) {
            auto value = prop.getter(component);
            if (value) {
                // Try to convert common types
                if (auto* val_f = value.try_cast<float>()) {
                    result[prop.name] = *val_f;
                } else if (auto* val_d = value.try_cast<double>()) {
                    result[prop.name] = *val_d;
                } else if (auto* val_i = value.try_cast<int>()) {
                    result[prop.name] = *val_i;
                } else if (auto* val_b = value.try_cast<bool>()) {
                    result[prop.name] = *val_b;
                } else if (auto* val_s = value.try_cast<std::string>()) {
                    result[prop.name] = *val_s;
                } else if (auto* val_v = value.try_cast<Vec3>()) {
                    result[prop.name] = *val_v;
                } else if (auto* val_q = value.try_cast<Quat>()) {
                    result[prop.name] = *val_q;
                }
            }
        }

        return result;
    });

    // Add a component by type name
    engine.set_function("add_component", [](uint32_t entity_id, const std::string& type_name) -> bool {
        auto* world = get_current_script_world();
        if (!world) {
            core::log(core::LogLevel::Warn, "add_component called outside script context");
            return false;
        }

        auto& registry = world->registry();
        auto entity = static_cast<entt::entity>(entity_id);

        if (!registry.valid(entity)) {
            return false;
        }

        auto& type_reg = reflect::TypeRegistry::instance();
        return type_reg.add_component_any(registry, entity, type_name);
    });

    // Remove a component by type name
    engine.set_function("remove_component", [](uint32_t entity_id, const std::string& type_name) -> bool {
        auto* world = get_current_script_world();
        if (!world) {
            core::log(core::LogLevel::Warn, "remove_component called outside script context");
            return false;
        }

        auto& registry = world->registry();
        auto entity = static_cast<entt::entity>(entity_id);

        if (!registry.valid(entity)) {
            return false;
        }

        auto& type_reg = reflect::TypeRegistry::instance();
        return type_reg.remove_component_any(registry, entity, type_name);
    });

    // NullEntity constant
    lua["NullEntity"] = NullEntity;
}

} // namespace engine::script
