#include <engine/plugin/hot_reload_manager.hpp>
#include <engine/core/log.hpp>
#include <engine/scene/scene_serializer.hpp>
#include <nlohmann/json.hpp>

namespace engine::plugin {

HotReloadManager::HotReloadManager() = default;

HotReloadManager::~HotReloadManager() {
    shutdown();
}

void HotReloadManager::init(const std::filesystem::path& dll_path,
                            GameContext* ctx,
                            SystemRegistry* registry,
                            const HotReloadConfig& config) {
    m_dll_path = dll_path;
    m_context = ctx;
    m_registry = registry;
    m_config = config;
    m_last_poll_time = std::chrono::steady_clock::now();

    // Get initial modified time
    m_last_modified = get_dll_modified_time();

    // Load the plugin (with copy for hot reload support)
    LoadResult result = m_loader.load(dll_path, m_config.enabled);
    if (result != LoadResult::Success) {
        core::log(core::LogLevel::Error, "Failed to load game plugin: {}",
                  load_result_to_string(result));
        return;
    }

    // Initialize the plugin
    m_loader.call_register_components();

    if (!m_loader.call_init(ctx)) {
        core::log(core::LogLevel::Error, "Game plugin initialization failed");
        m_loader.unload();
        return;
    }

    m_loader.call_register_systems(registry);

    m_initialized = true;
    core::log(core::LogLevel::Info, "Hot reload manager initialized for: {}", dll_path.string());
}

void HotReloadManager::shutdown() {
    if (m_initialized && m_loader.is_loaded()) {
        m_loader.call_shutdown();
        m_loader.unload();
    }
    m_initialized = false;
}

void HotReloadManager::poll() {
    if (!m_config.enabled || !m_initialized) {
        return;
    }

    // Throttle polling
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_poll_time).count();

    if (elapsed < m_config.poll_interval_ms) {
        return;
    }
    m_last_poll_time = now;

    // Check if DLL has been modified
    auto current_modified = get_dll_modified_time();
    if (current_modified != m_last_modified) {
        core::log(core::LogLevel::Info, "Game DLL changed, triggering hot reload...");
        m_last_modified = current_modified;

        // Small delay to ensure file write is complete
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        do_reload();
    }
}

void HotReloadManager::reload() {
    if (!m_initialized) {
        return;
    }
    do_reload();
}

void HotReloadManager::set_enabled(bool enabled) {
    m_config.enabled = enabled;
}

void HotReloadManager::set_config(const HotReloadConfig& config) {
    m_config = config;
}

void HotReloadManager::do_reload() {
    auto start_time = std::chrono::high_resolution_clock::now();

    core::log(core::LogLevel::Info, "=== HOT RELOAD START ===");

    nlohmann::json game_state;
    nlohmann::json world_state;

    // Step 1: Call pre_reload on game
    m_loader.call_pre_reload(m_context->world, &game_state);

    // Step 2: Serialize world state if enabled
    if (m_config.preserve_state) {
        if (!serialize_world_state(world_state)) {
            core::log(core::LogLevel::Warning, "Failed to serialize world state");
        }
    }

    // Step 3: Clear game systems
    m_registry->clear_game_systems();

    // Step 4: Shutdown and unload old plugin
    m_loader.call_shutdown();
    m_loader.unload();

    // Step 5: Load new plugin (with copy)
    LoadResult result = m_loader.load(m_dll_path, true);
    if (result != LoadResult::Success) {
        core::log(core::LogLevel::Error, "Hot reload failed: {}", load_result_to_string(result));

        if (m_callback) {
            m_callback(false, load_result_to_string(result));
        }
        return;
    }

    // Step 6: Register components
    m_loader.call_register_components();

    // Step 7: Initialize new plugin
    if (!m_loader.call_init(m_context)) {
        core::log(core::LogLevel::Error, "Hot reload failed: plugin init returned false");

        if (m_callback) {
            m_callback(false, "Plugin initialization failed");
        }
        return;
    }

    // Step 8: Register new systems
    m_loader.call_register_systems(m_registry);

    // Step 9: Deserialize world state
    if (m_config.preserve_state && !world_state.empty()) {
        if (!deserialize_world_state(world_state)) {
            core::log(core::LogLevel::Warning, "Failed to deserialize world state");
        }
    }

    // Step 10: Call post_reload on game
    m_loader.call_post_reload(m_context->world, &game_state);

    auto end_time = std::chrono::high_resolution_clock::now();
    m_last_reload_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    m_reload_count++;

    core::log(core::LogLevel::Info, "=== HOT RELOAD COMPLETE ({:.2f}ms) ===", m_last_reload_time_ms);

    if (m_callback) {
        m_callback(true, "Reload successful");
    }
}

bool HotReloadManager::serialize_world_state(nlohmann::json& out) {
    if (!m_context || !m_context->world) {
        return false;
    }

    try {
        // Use scene serializer to save world state
        // This captures all entities and components registered with reflection
        out = scene::SceneSerializer::serialize(*m_context->world);
        return true;
    } catch (const std::exception& e) {
        core::log(core::LogLevel::Error, "World serialization failed: {}", e.what());
        return false;
    }
}

bool HotReloadManager::deserialize_world_state(const nlohmann::json& state) {
    if (!m_context || !m_context->world) {
        return false;
    }

    try {
        // Clear existing entities (they have stale component data)
        m_context->world->clear();

        // Deserialize saved state
        scene::SceneSerializer::deserialize(*m_context->world, state);
        return true;
    } catch (const std::exception& e) {
        core::log(core::LogLevel::Error, "World deserialization failed: {}", e.what());
        return false;
    }
}

std::filesystem::file_time_type HotReloadManager::get_dll_modified_time() const {
    try {
        if (std::filesystem::exists(m_dll_path)) {
            return std::filesystem::last_write_time(m_dll_path);
        }
    } catch (...) {
        // Ignore errors
    }
    return std::filesystem::file_time_type{};
}

} // namespace engine::plugin
