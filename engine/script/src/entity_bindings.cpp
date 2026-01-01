#include <engine/script/bindings.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/transform.hpp>
#include <engine/scene/entity.hpp>
#include <engine/reflect/reflect.hpp>

namespace engine::script {

// Note: Entity bindings require a World reference, which is typically
// provided through context. For now, we provide the basic structure.

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
    // These functions will be called with a world context set elsewhere
    auto engine = lua["engine"].get_or_create<sol::table>();

    // Placeholder for get_component - actual implementation would need World context
    engine.set_function("get_component", [](uint32_t entity_id, const std::string& type_name) -> sol::object {
        // This is a simplified version - real implementation needs world access
        // and would use the reflection system to get the component
        return sol::nil;
    });

    engine.set_function("has_component", [](uint32_t entity_id, const std::string& type_name) -> bool {
        // Placeholder
        return false;
    });

    engine.set_function("add_component", [](uint32_t entity_id, const std::string& type_name) -> sol::object {
        // Placeholder
        return sol::nil;
    });

    engine.set_function("remove_component", [](uint32_t entity_id, const std::string& type_name) {
        // Placeholder
    });

    // NullEntity constant
    lua["NullEntity"] = NullEntity;
}

} // namespace engine::script
