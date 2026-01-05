#include <engine/script/bindings.hpp>
#include <engine/script/script_context.hpp>
#include <engine/save/save_system.hpp>
#include <engine/core/log.hpp>

namespace engine::script {

void register_save_bindings(sol::state& lua) {
    using namespace engine::save;
    using namespace engine::core;

    // SaveGameMetadata usertype (read-only)
    lua.new_usertype<SaveGameMetadata>("SaveGameMetadata",
        sol::constructors<>(),
        "name", sol::readonly(&SaveGameMetadata::name),
        "description", sol::readonly(&SaveGameMetadata::description),
        "timestamp", sol::readonly(&SaveGameMetadata::timestamp),
        "play_time_seconds", sol::readonly(&SaveGameMetadata::play_time_seconds),
        "level_name", sol::readonly(&SaveGameMetadata::level_name),
        "version", sol::readonly(&SaveGameMetadata::version),
        "get_date_string", &SaveGameMetadata::get_date_string,
        "get_play_time_string", &SaveGameMetadata::get_play_time_string
    );

    // SaveResult usertype
    lua.new_usertype<SaveResult>("SaveResult",
        sol::constructors<>(),
        "success", sol::readonly(&SaveResult::success),
        "error_message", sol::readonly(&SaveResult::error_message),
        "slot_name", sol::readonly(&SaveResult::slot_name),
        "save_time_ms", sol::readonly(&SaveResult::save_time_ms)
    );

    // LoadResult usertype
    lua.new_usertype<LoadResult>("LoadResult",
        sol::constructors<>(),
        "success", sol::readonly(&LoadResult::success),
        "error_message", sol::readonly(&LoadResult::error_message),
        "slot_name", sol::readonly(&LoadResult::slot_name),
        "load_time_ms", sol::readonly(&LoadResult::load_time_ms),
        "entities_loaded", sol::readonly(&LoadResult::entities_loaded)
    );

    // Create Save table
    auto save = lua.create_named_table("Save");

    // --- Synchronous Save Operations ---

    save.set_function("save_game", [](const std::string& slot_name) -> SaveResult {
        auto* world = get_current_script_world();
        if (!world) {
            SaveResult result;
            result.success = false;
            result.error_message = "No world context available";
            return result;
        }
        return get_save_system().save_game(*world, slot_name);
    });

    save.set_function("load_game", [](const std::string& slot_name) -> LoadResult {
        auto* world = get_current_script_world();
        if (!world) {
            LoadResult result;
            result.success = false;
            result.error_message = "No world context available";
            return result;
        }
        return get_save_system().load_game(*world, slot_name);
    });

    save.set_function("quick_save", []() -> SaveResult {
        auto* world = get_current_script_world();
        if (!world) {
            SaveResult result;
            result.success = false;
            result.error_message = "No world context available";
            return result;
        }
        return get_save_system().quick_save(*world);
    });

    save.set_function("quick_load", []() -> LoadResult {
        auto* world = get_current_script_world();
        if (!world) {
            LoadResult result;
            result.success = false;
            result.error_message = "No world context available";
            return result;
        }
        return get_save_system().quick_load(*world);
    });

    // --- Save Slot Management ---

    save.set_function("list_saves", []() -> std::vector<SaveGameMetadata> {
        return get_save_system().list_saves();
    });

    save.set_function("delete_save", [](const std::string& slot_name) -> bool {
        return get_save_system().delete_save(slot_name);
    });

    save.set_function("save_exists", [](const std::string& slot_name) -> bool {
        return get_save_system().save_exists(slot_name);
    });

    save.set_function("get_save_path", [](const std::string& slot_name) -> std::string {
        return get_save_system().get_save_path(slot_name);
    });

    // --- Progress/Status Queries ---

    save.set_function("is_saving", []() -> bool {
        return get_save_system().is_saving();
    });

    save.set_function("is_loading", []() -> bool {
        return get_save_system().is_loading();
    });

    save.set_function("get_save_progress", []() -> float {
        return get_save_system().get_save_progress();
    });

    save.set_function("get_load_progress", []() -> float {
        return get_save_system().get_load_progress();
    });

    // --- Autosave Control ---

    save.set_function("enable_autosave", [](bool enabled) {
        get_save_system().enable_autosave(enabled);
    });

    save.set_function("is_autosave_enabled", []() -> bool {
        return get_save_system().is_autosave_enabled();
    });

    save.set_function("trigger_autosave", []() {
        auto* world = get_current_script_world();
        if (world) {
            get_save_system().trigger_autosave(*world);
        }
    });

    // --- Play Time ---

    save.set_function("get_play_time", []() -> uint32_t {
        return get_save_system().get_current_play_time();
    });

    save.set_function("start_play_time_tracking", []() {
        get_save_system().start_play_time_tracking();
    });

    save.set_function("pause_play_time_tracking", []() {
        get_save_system().pause_play_time_tracking();
    });
}

} // namespace engine::script
