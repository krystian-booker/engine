#pragma once

#include <engine/plugin/plugin_loader.hpp>
#include <engine/plugin/system_registry.hpp>
#include <engine/scene/world.hpp>
#include <filesystem>
#include <functional>
#include <chrono>
#include <nlohmann/json_fwd.hpp>

namespace engine::plugin {

// Hot reload configuration
struct HotReloadConfig {
    bool enabled = true;              // Master toggle
    bool preserve_state = true;       // Serialize/deserialize world state
    float poll_interval_ms = 500.0f;  // How often to check for changes
};

// Hot reload manager handles watching and reloading game DLLs
class HotReloadManager {
public:
    HotReloadManager();
    ~HotReloadManager();

    // Initialize with DLL path and game context
    void init(const std::filesystem::path& dll_path,
              GameContext* ctx,
              SystemRegistry* registry,
              const HotReloadConfig& config = {});

    // Shutdown and cleanup
    void shutdown();

    // Check for DLL changes and reload if needed (call each frame)
    void poll();

    // Force a reload
    void reload();

    // Enable/disable hot reload at runtime
    void set_enabled(bool enabled);
    bool is_enabled() const { return m_config.enabled; }

    // Set configuration
    void set_config(const HotReloadConfig& config);
    const HotReloadConfig& get_config() const { return m_config; }

    // Get current plugin loader
    PluginLoader& get_loader() { return m_loader; }
    const PluginLoader& get_loader() const { return m_loader; }

    // Callback for reload events
    using ReloadCallback = std::function<void(bool success, const std::string& message)>;
    void set_reload_callback(ReloadCallback callback) { m_callback = std::move(callback); }

    // Statistics
    uint32_t get_reload_count() const { return m_reload_count; }
    double get_last_reload_time_ms() const { return m_last_reload_time_ms; }

private:
    void do_reload();
    bool serialize_world_state(nlohmann::json& out);
    bool deserialize_world_state(const nlohmann::json& state);
    std::filesystem::file_time_type get_dll_modified_time() const;

    PluginLoader m_loader;
    std::filesystem::path m_dll_path;
    std::filesystem::file_time_type m_last_modified;

    GameContext* m_context = nullptr;
    SystemRegistry* m_registry = nullptr;
    HotReloadConfig m_config;

    ReloadCallback m_callback;

    std::chrono::steady_clock::time_point m_last_poll_time;
    uint32_t m_reload_count = 0;
    double m_last_reload_time_ms = 0.0;

    bool m_initialized = false;
};

} // namespace engine::plugin
