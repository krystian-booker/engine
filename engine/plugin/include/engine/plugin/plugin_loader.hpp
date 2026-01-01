#pragma once

#include <engine/plugin/game_interface.hpp>
#include <string>
#include <filesystem>

namespace engine::plugin {

// Result of loading a plugin
enum class LoadResult {
    Success,
    FileNotFound,
    LoadFailed,
    MissingExports,
    VersionMismatch,
    InitFailed
};

const char* load_result_to_string(LoadResult result);

// Loaded plugin handle with function pointers
class PluginLoader {
public:
    PluginLoader();
    ~PluginLoader();

    // Non-copyable, movable
    PluginLoader(const PluginLoader&) = delete;
    PluginLoader& operator=(const PluginLoader&) = delete;
    PluginLoader(PluginLoader&& other) noexcept;
    PluginLoader& operator=(PluginLoader&& other) noexcept;

    // Load a game DLL
    // If copy_before_load is true, copies DLL to a temp file first (for hot reload)
    LoadResult load(const std::filesystem::path& dll_path, bool copy_before_load = false);

    // Unload the current DLL
    void unload();

    // Check if a DLL is loaded
    bool is_loaded() const { return m_handle != nullptr; }

    // Get the loaded DLL path
    const std::filesystem::path& get_path() const { return m_dll_path; }

    // Get plugin info (call after load)
    PluginInfo get_info() const;

    // Access function pointers (only valid after successful load)
    GetInfoFn get_info_fn() const { return m_get_info; }
    InitFn init_fn() const { return m_init; }
    RegisterSystemsFn register_systems_fn() const { return m_register_systems; }
    RegisterComponentsFn register_components_fn() const { return m_register_components; }
    PreReloadFn pre_reload_fn() const { return m_pre_reload; }
    PostReloadFn post_reload_fn() const { return m_post_reload; }
    ShutdownFn shutdown_fn() const { return m_shutdown; }

    // Convenience methods that call the function pointers
    bool call_init(GameContext* ctx);
    void call_register_systems(SystemRegistry* reg);
    void call_register_components();
    void call_pre_reload(scene::World* world, void* state);
    void call_post_reload(scene::World* world, const void* state);
    void call_shutdown();

private:
    void* get_proc(const char* name);
    void reset();

    void* m_handle = nullptr;
    std::filesystem::path m_dll_path;
    std::filesystem::path m_loaded_path;  // Actual loaded path (may be temp copy)

    // Function pointers
    GetInfoFn m_get_info = nullptr;
    InitFn m_init = nullptr;
    RegisterSystemsFn m_register_systems = nullptr;
    RegisterComponentsFn m_register_components = nullptr;
    PreReloadFn m_pre_reload = nullptr;
    PostReloadFn m_post_reload = nullptr;
    ShutdownFn m_shutdown = nullptr;
};

} // namespace engine::plugin
