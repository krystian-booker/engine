#include <engine/script/bindings.hpp>
#include <engine/script/script_context.hpp>
#include <engine/scene/scene_serializer.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/entity.hpp>
#include <engine/core/log.hpp>
#include <engine/core/scene_transition.hpp>

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

    // --- Scene Transitions ---

    scene.set_function("transition_to", [](const std::string& scene_path,
                                            sol::optional<sol::table> options) {
        using namespace engine::core;

        TransitionSettings settings;

        if (options.has_value()) {
            auto& opts = options.value();

            if (opts["fade_out_duration"].valid()) {
                settings.fade_out_duration = opts["fade_out_duration"].get<float>();
            }
            if (opts["fade_in_duration"].valid()) {
                settings.fade_in_duration = opts["fade_in_duration"].get<float>();
            }
            if (opts["hold_duration"].valid()) {
                settings.hold_duration = opts["hold_duration"].get<float>();
            }
            if (opts["type"].valid()) {
                settings.type = static_cast<TransitionType>(opts["type"].get<int>());
            }
            if (opts["color"].valid()) {
                auto color = opts["color"].get<sol::table>();
                settings.fade_color = Vec4{
                    color[1].get_or(0.0f),
                    color[2].get_or(0.0f),
                    color[3].get_or(0.0f),
                    color[4].get_or(1.0f)
                };
            }
        }

        scene_transitions().transition_to(scene_path, settings);
    });

    scene.set_function("begin_transition", [](sol::optional<sol::table> options) {
        using namespace engine::core;

        TransitionSettings settings;

        if (options.has_value()) {
            auto& opts = options.value();

            if (opts["fade_out_duration"].valid()) {
                settings.fade_out_duration = opts["fade_out_duration"].get<float>();
            }
            if (opts["fade_in_duration"].valid()) {
                settings.fade_in_duration = opts["fade_in_duration"].get<float>();
            }
            if (opts["hold_duration"].valid()) {
                settings.hold_duration = opts["hold_duration"].get<float>();
            }
            if (opts["type"].valid()) {
                settings.type = static_cast<TransitionType>(opts["type"].get<int>());
            }
        }

        scene_transitions().begin_transition(settings);
    });

    scene.set_function("end_transition", []() {
        engine::core::scene_transitions().end_transition();
    });

    scene.set_function("is_transitioning", []() -> bool {
        return engine::core::scene_transitions().is_transitioning();
    });

    scene.set_function("get_transition_phase", []() -> int {
        return static_cast<int>(engine::core::scene_transitions().get_phase());
    });

    scene.set_function("get_fade_alpha", []() -> float {
        return engine::core::scene_transitions().get_fade_alpha();
    });

    scene.set_function("set_loading_progress", [](float progress) {
        engine::core::scene_transitions().set_loading_progress(progress);
    });

    // Transition type constants
    scene["TRANSITION_NONE"] = static_cast<int>(core::TransitionType::None);
    scene["TRANSITION_FADE"] = static_cast<int>(core::TransitionType::Fade);
    scene["TRANSITION_FADE_WHITE"] = static_cast<int>(core::TransitionType::FadeWhite);
    scene["TRANSITION_FADE_COLOR"] = static_cast<int>(core::TransitionType::FadeColor);
    scene["TRANSITION_CROSSFADE"] = static_cast<int>(core::TransitionType::Crossfade);

    // Transition phase constants
    scene["PHASE_IDLE"] = static_cast<int>(core::TransitionPhase::Idle);
    scene["PHASE_FADING_OUT"] = static_cast<int>(core::TransitionPhase::FadingOut);
    scene["PHASE_LOADING"] = static_cast<int>(core::TransitionPhase::Loading);
    scene["PHASE_FADING_IN"] = static_cast<int>(core::TransitionPhase::FadingIn);
}

} // namespace engine::script
