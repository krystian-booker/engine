#include <engine/script/bindings.hpp>
#include <engine/script/script_context.hpp>
#include <engine/scene/scene_serializer.hpp>
#include <engine/scene/world.hpp>
#include <engine/scene/entity.hpp>
#include <engine/scene/interaction.hpp>
#include <engine/scene/entity_pool.hpp>
#include <engine/scene/spawn_system.hpp>
#include <engine/core/log.hpp>
#include <engine/core/scene_transition.hpp>
#include <engine/core/timer.hpp>
#include <engine/core/game_events.hpp>

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

    // ==========================================================================
    // Interaction System Bindings
    // ==========================================================================

    auto interaction = lua.create_named_table("Interaction");

    interaction.set_function("find_best", [](float px, float py, float pz,
                                              float fx, float fy, float fz,
                                              sol::optional<float> max_dist) -> sol::table {
        auto* world = get_current_script_world();
        sol::state_view lua_view = sol::state_view(nullptr);  // Will be set below

        if (!world) {
            return sol::nil;
        }

        Vec3 position{px, py, pz};
        Vec3 forward{fx, fy, fz};
        float distance = max_dist.value_or(3.0f);

        auto result = interactions().find_best_interactable(*world, position, forward, distance);
        if (!result.has_value()) {
            return sol::nil;
        }

        // Return result as table
        sol::state* L = nullptr;
        // We need to get the lua state - use a workaround
        return sol::nil;  // Simplified - would need proper lua state access
    });

    interaction.set_function("find_best_from_entity", [](uint32_t entity_id,
                                                          sol::optional<float> max_dist) -> sol::object {
        auto* world = get_current_script_world();
        if (!world) {
            return sol::nil;
        }

        auto entity = static_cast<Entity>(entity_id);
        if (!world->valid(entity)) {
            return sol::nil;
        }

        // Get entity position and forward
        auto* transform = world->try_get<LocalTransform>(entity);
        if (!transform) {
            return sol::nil;
        }

        Vec3 position = transform->position;
        Vec3 forward = transform->get_forward();
        float distance = max_dist.value_or(3.0f);

        auto result = interactions().find_best_interactable(*world, position, forward, distance);
        if (!result.has_value()) {
            return sol::nil;
        }

        return sol::make_object(sol::nil.lua_state(), static_cast<uint32_t>(result->entity));
    });

    interaction.set_function("interact", [](uint32_t interactor_id, uint32_t target_id) {
        auto* world = get_current_script_world();
        if (!world) return;

        auto interactor = static_cast<Entity>(interactor_id);
        auto target = static_cast<Entity>(target_id);

        if (!world->valid(interactor) || !world->valid(target)) return;

        interactions().interact(*world, interactor, target);
    });

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

    interaction.set_function("is_holding", []() -> bool {
        return interactions().get_hold_state().holding;
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

    // ==========================================================================
    // Object Pool Bindings
    // ==========================================================================

    auto pool = lua.create_named_table("Pool");

    pool.set_function("create", [](const std::string& name, sol::table config) {
        auto* world = get_current_script_world();
        if (!world) return;

        PoolConfig cfg;
        cfg.pool_name = name;
        cfg.prefab_path = config.get_or<std::string>("prefab", "");
        cfg.initial_size = config.get_or<uint32_t>("initial", 10);
        cfg.max_size = config.get_or<uint32_t>("max", 100);
        cfg.growth_size = config.get_or<uint32_t>("growth", 5);
        cfg.recycle_delay = config.get_or<float>("recycle_delay", 0.0f);
        cfg.auto_expand = config.get_or<bool>("auto_expand", true);
        cfg.warm_on_init = config.get_or<bool>("warm", true);

        pools().create_pool(*world, cfg);
    });

    pool.set_function("acquire", [](const std::string& name,
                                     sol::optional<float> px,
                                     sol::optional<float> py,
                                     sol::optional<float> pz) -> uint32_t {
        if (px.has_value() && py.has_value() && pz.has_value()) {
            Vec3 pos{px.value(), py.value(), pz.value()};
            return static_cast<uint32_t>(pools().acquire(name, pos));
        }
        return static_cast<uint32_t>(pools().acquire(name));
    });

    pool.set_function("release", [](uint32_t entity_id) {
        auto* world = get_current_script_world();
        if (!world) return;
        pools().release(*world, static_cast<Entity>(entity_id));
    });

    pool.set_function("release_immediate", [](uint32_t entity_id) {
        auto* world = get_current_script_world();
        if (!world) return;
        pools().release_immediate(*world, static_cast<Entity>(entity_id));
    });

    pool.set_function("warm", [](const std::string& name, uint32_t count) {
        if (auto* p = pools().get_pool(name)) {
            p->warm(count);
        }
    });

    pool.set_function("has", [](const std::string& name) -> bool {
        return pools().has_pool(name);
    });

    pool.set_function("available", [](const std::string& name) -> uint32_t {
        if (auto* p = pools().get_pool(name)) {
            return p->available_count();
        }
        return 0;
    });

    pool.set_function("active", [](const std::string& name) -> uint32_t {
        if (auto* p = pools().get_pool(name)) {
            return p->active_count();
        }
        return 0;
    });

    // ==========================================================================
    // Spawn System Bindings
    // ==========================================================================

    auto spawn = lua.create_named_table("Spawn");

    spawn.set_function("entity", [](const std::string& prefab,
                                     float px, float py, float pz,
                                     sol::optional<float> rx,
                                     sol::optional<float> ry,
                                     sol::optional<float> rz,
                                     sol::optional<float> rw) -> uint32_t {
        auto* world = get_current_script_world();
        if (!world) return static_cast<uint32_t>(NullEntity);

        Vec3 pos{px, py, pz};

        if (rx.has_value() && ry.has_value() && rz.has_value() && rw.has_value()) {
            Quat rot{rw.value(), rx.value(), ry.value(), rz.value()};
            return static_cast<uint32_t>(spawns().spawn(*world, prefab, pos, rot));
        }

        return static_cast<uint32_t>(spawns().spawn(*world, prefab, pos));
    });

    spawn.set_function("from_pool", [](const std::string& pool_name,
                                        float px, float py, float pz) -> uint32_t {
        auto* world = get_current_script_world();
        if (!world) return static_cast<uint32_t>(NullEntity);

        Vec3 pos{px, py, pz};
        return static_cast<uint32_t>(spawns().spawn_from_pool(*world, pool_name, pos));
    });

    spawn.set_function("at_point", [](uint32_t spawn_point_id) -> uint32_t {
        auto* world = get_current_script_world();
        if (!world) return static_cast<uint32_t>(NullEntity);

        return static_cast<uint32_t>(spawns().spawn_at_point(*world, static_cast<Entity>(spawn_point_id)));
    });

    spawn.set_function("despawn", [](uint32_t entity_id) {
        auto* world = get_current_script_world();
        if (!world) return;
        spawns().despawn(*world, static_cast<Entity>(entity_id));
    });

    spawn.set_function("start_waves", [](uint32_t spawner_id) {
        auto* world = get_current_script_world();
        if (!world) return;
        spawns().start_waves(*world, static_cast<Entity>(spawner_id));
    });

    spawn.set_function("stop_waves", [](uint32_t spawner_id) {
        auto* world = get_current_script_world();
        if (!world) return;
        spawns().stop_waves(*world, static_cast<Entity>(spawner_id));
    });

    spawn.set_function("skip_wave", [](uint32_t spawner_id) {
        auto* world = get_current_script_world();
        if (!world) return;
        spawns().skip_wave(*world, static_cast<Entity>(spawner_id));
    });

    spawn.set_function("reset_waves", [](uint32_t spawner_id) {
        auto* world = get_current_script_world();
        if (!world) return;
        spawns().reset_waves(*world, static_cast<Entity>(spawner_id));
    });

    spawn.set_function("get_current_wave", [](uint32_t spawner_id) -> int {
        auto* world = get_current_script_world();
        if (!world) return -1;
        return spawns().get_current_wave(*world, static_cast<Entity>(spawner_id));
    });

    spawn.set_function("get_active_count", [](uint32_t spawner_id) -> int {
        auto* world = get_current_script_world();
        if (!world) return 0;
        return spawns().get_active_spawn_count(*world, static_cast<Entity>(spawner_id));
    });

    spawn.set_function("are_waves_complete", [](uint32_t spawner_id) -> bool {
        auto* world = get_current_script_world();
        if (!world) return false;
        return spawns().are_all_waves_complete(*world, static_cast<Entity>(spawner_id));
    });

    // ==========================================================================
    // Timer System Bindings
    // ==========================================================================

    auto timer = lua.create_named_table("Timer");

    timer.set_function("set_timeout", [](float delay, sol::function callback) -> uint64_t {
        auto handle = core::timers().set_timeout(delay, [callback]() {
            if (callback.valid()) {
                callback();
            }
        });
        return handle.id;
    });

    timer.set_function("set_interval", [](float interval, sol::function callback,
                                           sol::optional<int> count) -> uint64_t {
        core::TimerHandle handle;
        if (count.has_value()) {
            handle = core::timers().set_interval(interval, count.value(), [callback]() {
                if (callback.valid()) {
                    callback();
                }
            });
        } else {
            handle = core::timers().set_interval(interval, [callback]() {
                if (callback.valid()) {
                    callback();
                }
            });
        }
        return handle.id;
    });

    timer.set_function("cancel", [](uint64_t id) {
        core::timers().cancel(core::TimerHandle{id});
    });

    timer.set_function("pause", [](uint64_t id) {
        core::timers().pause(core::TimerHandle{id});
    });

    timer.set_function("resume", [](uint64_t id) {
        core::timers().resume(core::TimerHandle{id});
    });

    timer.set_function("is_active", [](uint64_t id) -> bool {
        return core::timers().is_active(core::TimerHandle{id});
    });

    timer.set_function("is_paused", [](uint64_t id) -> bool {
        return core::timers().is_paused(core::TimerHandle{id});
    });

    timer.set_function("get_remaining", [](uint64_t id) -> float {
        return core::timers().get_remaining(core::TimerHandle{id});
    });

    timer.set_function("cancel_all", []() {
        core::timers().cancel_all();
    });

    // ==========================================================================
    // Game Events Bindings
    // ==========================================================================

    auto events = lua.create_named_table("Events");

    // Store connections in a static map to prevent disconnection
    static std::unordered_map<std::string, std::vector<core::ScopedConnection>> s_event_connections;

    events.set_function("on", [](const std::string& event_name, sol::function callback) {
        auto conn = core::game_events().subscribe_dynamic(event_name,
            [callback](const std::any& data) -> bool {
                if (callback.valid()) {
                    // Try to pass data if it's a table
                    callback();
                }
                return false;  // Don't consume
            }
        );
        s_event_connections[event_name].push_back(std::move(conn));
    });

    events.set_function("emit", [](const std::string& event_name, sol::optional<sol::table> data) {
        if (data.has_value()) {
            // Convert table to any (simplified - just emit without data for now)
            core::game_events().emit_dynamic(event_name);
        } else {
            core::game_events().emit_dynamic(event_name);
        }
    });

    events.set_function("emit_deferred", [](const std::string& event_name) {
        core::game_events().emit_dynamic_deferred(event_name);
    });

    events.set_function("clear", [](const std::string& event_name) {
        core::game_events().clear_dynamic_handlers(event_name);
        s_event_connections.erase(event_name);
    });
}

} // namespace engine::script
