#include <engine/script/bindings.hpp>
#include <engine/script/script_context.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <engine/scene/entity.hpp>
#include <engine/scene/interaction.hpp>
#include <engine/reflect/reflect.hpp>
#include <engine/reflect/type_registry.hpp>
#include <engine/core/log.hpp>

// For hierarchy functions
using engine::scene::Hierarchy;
using engine::scene::set_parent;
using engine::scene::remove_parent;
using engine::scene::get_children;

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

    // --- Entity Creation and Destruction ---

    // Create a new entity
    engine.set_function("create_entity", [](sol::optional<std::string> name) -> uint32_t {
        auto* world = get_current_script_world();
        if (!world) {
            core::log(core::LogLevel::Warn, "engine.create_entity called outside script context");
            return static_cast<uint32_t>(NullEntity);
        }

        Entity entity;
        if (name.has_value()) {
            entity = world->create(name.value());
        } else {
            entity = world->create();
        }

        return static_cast<uint32_t>(entity);
    });

    // Destroy an entity
    engine.set_function("destroy_entity", [](uint32_t entity_id) {
        auto* world = get_current_script_world();
        if (!world) {
            core::log(core::LogLevel::Warn, "engine.destroy_entity called outside script context");
            return;
        }

        auto entity = static_cast<entt::entity>(entity_id);
        if (world->registry().valid(entity)) {
            world->destroy(entity);
        }
    });

    // Check if entity is valid
    engine.set_function("is_valid", [](uint32_t entity_id) -> bool {
        auto* world = get_current_script_world();
        if (!world) {
            return false;
        }

        auto entity = static_cast<entt::entity>(entity_id);
        return world->valid(entity);
    });

    // Find entity by name
    engine.set_function("find_entity", [](const std::string& name) -> uint32_t {
        auto* world = get_current_script_world();
        if (!world) {
            return static_cast<uint32_t>(NullEntity);
        }

        Entity entity = world->find_by_name(name);
        return static_cast<uint32_t>(entity);
    });

    // --- Hierarchy Functions ---

    // Get parent of entity
    engine.set_function("get_parent", [](uint32_t entity_id) -> uint32_t {
        auto* world = get_current_script_world();
        if (!world) {
            return static_cast<uint32_t>(NullEntity);
        }

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) {
            return static_cast<uint32_t>(NullEntity);
        }

        auto* hierarchy = world->try_get<Hierarchy>(entity);
        if (!hierarchy) {
            return static_cast<uint32_t>(NullEntity);
        }

        return static_cast<uint32_t>(hierarchy->parent);
    });

    // Get children of entity
    engine.set_function("get_children", [](uint32_t entity_id) -> std::vector<uint32_t> {
        auto* world = get_current_script_world();
        if (!world) {
            return {};
        }

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) {
            return {};
        }

        const auto& children = get_children(*world, entity);
        std::vector<uint32_t> result;
        result.reserve(children.size());
        for (auto child : children) {
            result.push_back(static_cast<uint32_t>(child));
        }
        return result;
    });

    // Set parent of entity
    engine.set_function("set_parent", [](uint32_t child_id, uint32_t parent_id) {
        auto* world = get_current_script_world();
        if (!world) {
            return;
        }

        auto child = static_cast<entt::entity>(child_id);
        auto parent = static_cast<entt::entity>(parent_id);

        if (!world->registry().valid(child)) {
            return;
        }

        if (parent == NullEntity) {
            remove_parent(*world, child);
        } else if (world->registry().valid(parent)) {
            set_parent(*world, child, parent);
        }
    });

    // Remove parent (make root entity)
    engine.set_function("remove_parent", [](uint32_t entity_id) {
        auto* world = get_current_script_world();
        if (!world) {
            return;
        }

        auto entity = static_cast<entt::entity>(entity_id);
        if (world->registry().valid(entity)) {
            remove_parent(*world, entity);
        }
    });

    // Get entity name
    engine.set_function("get_name", [](uint32_t entity_id) -> std::string {
        auto* world = get_current_script_world();
        if (!world) {
            return "";
        }

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) {
            return "";
        }

        auto* info = world->try_get<EntityInfo>(entity);
        if (!info) {
            return "";
        }

        return info->name;
    });

    // Set entity name
    engine.set_function("set_name", [](uint32_t entity_id, const std::string& name) {
        auto* world = get_current_script_world();
        if (!world) {
            return;
        }

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) {
            return;
        }

        auto* info = world->try_get<EntityInfo>(entity);
        if (info) {
            info->name = name;
        }
    });

    // Enable/disable entity
    engine.set_function("set_enabled", [](uint32_t entity_id, bool enabled) {
        auto* world = get_current_script_world();
        if (!world) {
            return;
        }

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) {
            return;
        }

        auto* info = world->try_get<EntityInfo>(entity);
        if (info) {
            info->enabled = enabled;
        }
    });

    // Check if entity is enabled
    engine.set_function("is_enabled", [](uint32_t entity_id) -> bool {
        auto* world = get_current_script_world();
        if (!world) {
            return false;
        }

        auto entity = static_cast<entt::entity>(entity_id);
        if (!world->registry().valid(entity)) {
            return false;
        }

        auto* info = world->try_get<EntityInfo>(entity);
        return info ? info->enabled : true;
    });

    // --- Entity Query Functions ---

    // Find all entities with a specific component type
    engine.set_function("find_entities_with_component", [](const std::string& type_name) -> std::vector<uint32_t> {
        auto* world = get_current_script_world();
        if (!world) {
            core::log(core::LogLevel::Warn, "find_entities_with_component called outside script context");
            return {};
        }

        std::vector<uint32_t> result;
        auto& registry = world->registry();
        auto& type_reg = reflect::TypeRegistry::instance();

        // Handle built-in types first for efficiency
        if (type_name == "LocalTransform") {
            auto view = registry.view<LocalTransform>();
            result.reserve(view.size());
            for (auto entity : view) {
                result.push_back(static_cast<uint32_t>(entity));
            }
            return result;
        }
        if (type_name == "WorldTransform") {
            auto view = registry.view<WorldTransform>();
            result.reserve(view.size());
            for (auto entity : view) {
                result.push_back(static_cast<uint32_t>(entity));
            }
            return result;
        }
        if (type_name == "EntityInfo") {
            auto view = registry.view<EntityInfo>();
            result.reserve(view.size());
            for (auto entity : view) {
                result.push_back(static_cast<uint32_t>(entity));
            }
            return result;
        }

        // For reflected types, use the type registry
        // This iterates all entities and checks if they have the component
        for (auto entity : registry.view<entt::entity>()) {
            if (registry.valid(entity)) {
                auto component = type_reg.get_component_any(registry, entity, type_name);
                if (component) {
                    result.push_back(static_cast<uint32_t>(entity));
                }
            }
        }

        return result;
    });

    // Find all entities whose name contains the given substring
    engine.set_function("find_entities_by_name", [](const std::string& name_pattern) -> std::vector<uint32_t> {
        auto* world = get_current_script_world();
        if (!world) {
            return {};
        }

        std::vector<uint32_t> result;
        auto& registry = world->registry();
        auto view = registry.view<EntityInfo>();

        for (auto entity : view) {
            const auto& info = view.get<EntityInfo>(entity);
            if (info.name.find(name_pattern) != std::string::npos) {
                result.push_back(static_cast<uint32_t>(entity));
            }
        }

        return result;
    });

    // Find all entities whose name starts with the given prefix
    engine.set_function("find_entities_with_prefix", [](const std::string& prefix) -> std::vector<uint32_t> {
        auto* world = get_current_script_world();
        if (!world) {
            return {};
        }

        std::vector<uint32_t> result;
        auto& registry = world->registry();
        auto view = registry.view<EntityInfo>();

        for (auto entity : view) {
            const auto& info = view.get<EntityInfo>(entity);
            if (info.name.compare(0, prefix.size(), prefix) == 0) {
                result.push_back(static_cast<uint32_t>(entity));
            }
        }

        return result;
    });

    // Get total entity count
    engine.set_function("get_entity_count", []() -> uint32_t {
        auto* world = get_current_script_world();
        if (!world) {
            return 0;
        }

        return static_cast<uint32_t>(world->registry().storage<entt::entity>().size());
    });

    // --- Interaction System ---

    auto interaction = lua.create_named_table("Interaction");

    // Find best interactable near a position
    interaction.set_function("find_best", [](float px, float py, float pz,
                                              float fx, float fy, float fz,
                                              sol::optional<float> max_dist) -> sol::table {
        auto* world = get_current_script_world();
        sol::state_view lua_view = sol::state_view(get_current_lua_state());
        sol::table result = lua_view.create_table();

        if (!world) {
            result["entity"] = static_cast<uint32_t>(NullEntity);
            return result;
        }

        Vec3 pos{px, py, pz};
        Vec3 fwd = glm::normalize(Vec3{fx, fy, fz});
        float dist = max_dist.value_or(3.0f);

        auto candidate = interactions().find_best_interactable(*world, pos, fwd, dist);
        if (candidate) {
            result["entity"] = static_cast<uint32_t>(candidate->entity);
            result["distance"] = candidate->distance;
            result["id"] = candidate->interaction_id;
            result["name"] = candidate->display_name;
            result["type"] = static_cast<int>(candidate->type);
            result["hold_to_interact"] = candidate->hold_to_interact;
            result["hold_duration"] = candidate->hold_duration;
        } else {
            result["entity"] = static_cast<uint32_t>(NullEntity);
        }

        return result;
    });

    // Find all interactables
    interaction.set_function("find_all", [](float px, float py, float pz,
                                             float fx, float fy, float fz,
                                             sol::optional<float> max_dist) -> std::vector<sol::table> {
        auto* world = get_current_script_world();
        std::vector<sol::table> results;

        if (!world) {
            return results;
        }

        sol::state_view lua_view = sol::state_view(get_current_lua_state());
        Vec3 pos{px, py, pz};
        Vec3 fwd = glm::normalize(Vec3{fx, fy, fz});
        float dist = max_dist.value_or(5.0f);

        auto candidates = interactions().find_all_interactables(*world, pos, fwd, dist);
        for (const auto& c : candidates) {
            sol::table t = lua_view.create_table();
            t["entity"] = static_cast<uint32_t>(c.entity);
            t["distance"] = c.distance;
            t["id"] = c.interaction_id;
            t["name"] = c.display_name;
            t["type"] = static_cast<int>(c.type);
            t["hold_to_interact"] = c.hold_to_interact;
            t["hold_duration"] = c.hold_duration;
            results.push_back(t);
        }

        return results;
    });

    // Perform interaction
    interaction.set_function("interact", [](uint32_t interactor_id, uint32_t target_id) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto interactor = static_cast<Entity>(interactor_id);
        auto target = static_cast<Entity>(target_id);
        interactions().interact(*world, interactor, target);
    });

    // Hold interaction management
    interaction.set_function("begin_hold", [](uint32_t interactor_id, uint32_t target_id) {
        auto interactor = static_cast<Entity>(interactor_id);
        auto target = static_cast<Entity>(target_id);
        interactions().begin_hold(interactor, target);
    });

    interaction.set_function("update_hold", [](float dt) -> bool {
        return interactions().update_hold(dt);
    });

    interaction.set_function("cancel_hold", []() {
        interactions().cancel_hold();
    });

    interaction.set_function("get_hold_progress", []() -> float {
        return interactions().get_hold_progress();
    });

    // Interaction type constants
    interaction["TYPE_GENERIC"] = static_cast<int>(InteractionType::Generic);
    interaction["TYPE_PICKUP"] = static_cast<int>(InteractionType::Pickup);
    interaction["TYPE_DOOR"] = static_cast<int>(InteractionType::Door);
    interaction["TYPE_LEVER"] = static_cast<int>(InteractionType::Lever);
    interaction["TYPE_TALK"] = static_cast<int>(InteractionType::Talk);
    interaction["TYPE_EXAMINE"] = static_cast<int>(InteractionType::Examine);
    interaction["TYPE_USE"] = static_cast<int>(InteractionType::Use);
    interaction["TYPE_CLIMB"] = static_cast<int>(InteractionType::Climb);
    interaction["TYPE_VEHICLE"] = static_cast<int>(InteractionType::Vehicle);
    interaction["TYPE_CUSTOM"] = static_cast<int>(InteractionType::Custom);
}

} // namespace engine::script
