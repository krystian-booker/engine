#include <engine/script/bindings.hpp>
#include <engine/script/script_context.hpp>
#include <engine/scene/scene_serializer.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/entity.hpp>
#include <engine/core/log.hpp>

namespace engine::script {

// Static serializer instance for the bindings
static engine::scene::SceneSerializer s_serializer;

void register_scene_bindings(sol::state& lua) {
    using namespace engine::scene;
    using namespace engine::core;

    // Create Scene table
    auto scene = lua.create_named_table("Scene");

    // --- Scene Loading/Saving ---

    scene.set_function("load", [](const std::string& path) -> bool {
        auto* world = get_current_script_world();
        if (!world) {
            core::log(core::LogLevel::Error, "Scene.load called without world context");
            return false;
        }
        return s_serializer.deserialize_from_file(*world, path);
    });

    scene.set_function("save", [](const std::string& path) -> bool {
        auto* world = get_current_script_world();
        if (!world) {
            core::log(core::LogLevel::Error, "Scene.save called without world context");
            return false;
        }
        return s_serializer.serialize_to_file(*world, path);
    });

    // --- Prefab/Template Instantiation ---

    scene.set_function("spawn_prefab", [](const std::string& prefab_path,
                                          sol::optional<uint32_t> parent_id) -> uint32_t {
        auto* world = get_current_script_world();
        if (!world) {
            core::log(core::LogLevel::Error, "Scene.spawn_prefab called without world context");
            return static_cast<uint32_t>(NullEntity);
        }

        Prefab prefab = Prefab::load(prefab_path);
        if (!prefab.valid()) {
            core::log(core::LogLevel::Error, "Failed to load prefab: {}", prefab_path);
            return static_cast<uint32_t>(NullEntity);
        }

        Entity parent = NullEntity;
        if (parent_id.has_value()) {
            parent = static_cast<Entity>(parent_id.value());
            if (!world->valid(parent)) {
                parent = NullEntity;
            }
        }

        Entity spawned = prefab.instantiate(*world, s_serializer, parent);
        return static_cast<uint32_t>(spawned);
    });

    // --- Entity Utilities ---

    scene.set_function("clone", [](uint32_t entity_id, sol::optional<uint32_t> parent_id) -> uint32_t {
        auto* world = get_current_script_world();
        if (!world) {
            return static_cast<uint32_t>(NullEntity);
        }

        auto source = static_cast<Entity>(entity_id);
        if (!world->valid(source)) {
            return static_cast<uint32_t>(NullEntity);
        }

        Entity parent = NullEntity;
        if (parent_id.has_value()) {
            parent = static_cast<Entity>(parent_id.value());
            if (!world->valid(parent)) {
                parent = NullEntity;
            }
        }

        Entity cloned = scene_utils::clone_entity(*world, source, parent);
        return static_cast<uint32_t>(cloned);
    });

    scene.set_function("find_by_path", [](const std::string& path) -> uint32_t {
        auto* world = get_current_script_world();
        if (!world) {
            return static_cast<uint32_t>(NullEntity);
        }

        Entity entity = scene_utils::find_entity_by_path(*world, path);
        return static_cast<uint32_t>(entity);
    });

    scene.set_function("get_entity_path", [](uint32_t entity_id) -> std::string {
        auto* world = get_current_script_world();
        if (!world) {
            return "";
        }

        auto entity = static_cast<Entity>(entity_id);
        if (!world->valid(entity)) {
            return "";
        }

        return scene_utils::get_entity_path(*world, entity);
    });

    scene.set_function("find_by_uuid", [](uint64_t uuid) -> uint32_t {
        auto* world = get_current_script_world();
        if (!world) {
            return static_cast<uint32_t>(NullEntity);
        }

        Entity entity = scene_utils::find_entity_by_uuid(*world, uuid);
        return static_cast<uint32_t>(entity);
    });

    scene.set_function("find_by_name", [](const std::string& name) -> uint32_t {
        auto* world = get_current_script_world();
        if (!world) {
            return static_cast<uint32_t>(NullEntity);
        }

        Entity entity = scene_utils::find_entity_by_name(*world, name);
        return static_cast<uint32_t>(entity);
    });

    scene.set_function("find_all_by_name", [](const std::string& name) -> std::vector<uint32_t> {
        auto* world = get_current_script_world();
        if (!world) {
            return {};
        }

        std::vector<Entity> entities = scene_utils::find_entities_by_name(*world, name);
        std::vector<uint32_t> result;
        result.reserve(entities.size());
        for (Entity e : entities) {
            result.push_back(static_cast<uint32_t>(e));
        }
        return result;
    });

    scene.set_function("count_entities", []() -> size_t {
        auto* world = get_current_script_world();
        if (!world) {
            return 0;
        }
        return scene_utils::count_entities(*world);
    });

    scene.set_function("delete_recursive", [](uint32_t entity_id) {
        auto* world = get_current_script_world();
        if (!world) {
            return;
        }

        auto entity = static_cast<Entity>(entity_id);
        if (world->valid(entity)) {
            scene_utils::delete_entity_recursive(*world, entity);
        }
    });

    // --- Prefab creation from entity ---

    scene.set_function("create_prefab_from_entity", [](uint32_t entity_id, const std::string& save_path) -> bool {
        auto* world = get_current_script_world();
        if (!world) {
            return false;
        }

        auto entity = static_cast<Entity>(entity_id);
        if (!world->valid(entity)) {
            return false;
        }

        Prefab prefab = Prefab::create_from_entity(*world, s_serializer, entity);
        if (!prefab.valid()) {
            return false;
        }

        return prefab.save(save_path);
    });

    // --- UUID generation ---

    scene.set_function("generate_uuid", []() -> uint64_t {
        return SceneSerializer::generate_uuid();
    });
}

} // namespace engine::script
