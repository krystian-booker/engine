#pragma once

#include <engine/asset/types.hpp>
#include <engine/render/renderer.hpp>
#include <memory>
#include <unordered_map>
#include <string>
#include <future>
#include <functional>
#include <shared_mutex>
#include <condition_variable>

namespace engine::asset {

// Asset loading status
enum class AssetStatus {
    NotLoaded,
    Loading,
    Loaded,
    Failed
};

// Asset manager - handles loading, caching, and hot-reloading of assets
class AssetManager {
public:
    AssetManager();
    ~AssetManager();

    // Initialize with renderer reference
    void init(render::IRenderer* renderer);
    void shutdown();

    // Synchronous loading
    std::shared_ptr<MeshAsset> load_mesh(const std::string& path);
    std::shared_ptr<TextureAsset> load_texture(const std::string& path);
    std::shared_ptr<ShaderAsset> load_shader(const std::string& path);
    std::shared_ptr<MaterialAsset> load_material(const std::string& path);
    std::shared_ptr<AudioAsset> load_audio(const std::string& path);
    std::shared_ptr<SceneAsset> load_scene(const std::string& path);
    std::shared_ptr<PrefabAsset> load_prefab(const std::string& path);

    // Asynchronous loading
    std::future<std::shared_ptr<MeshAsset>> load_mesh_async(const std::string& path);
    std::future<std::shared_ptr<TextureAsset>> load_texture_async(const std::string& path);
    std::future<std::shared_ptr<ShaderAsset>> load_shader_async(const std::string& path);
    std::future<std::shared_ptr<MaterialAsset>> load_material_async(const std::string& path);
    std::future<std::shared_ptr<AudioAsset>> load_audio_async(const std::string& path);
    std::future<std::shared_ptr<SceneAsset>> load_scene_async(const std::string& path);
    std::future<std::shared_ptr<PrefabAsset>> load_prefab_async(const std::string& path);

    // Generic load by extension
    std::shared_ptr<Asset> load(const std::string& path);

    // Check if asset is loaded
    bool is_loaded(const std::string& path) const;
    AssetStatus get_status(const std::string& path) const;

    // Hot reload support
    void enable_hot_reload(bool enabled);
    void poll_hot_reload();

    // Unload assets
    void unload(const std::string& path);
    void unload_unused();  // Unload assets with refcount == 1
    void unload_all();

    // Statistics
    size_t get_loaded_count() const;
    size_t get_memory_usage() const;

    // Reload callback
    using ReloadCallback = std::function<void(const std::string& path)>;
    void set_reload_callback(ReloadCallback callback);

private:
    // Internal loading functions
    std::shared_ptr<MeshAsset> load_mesh_internal(const std::string& path);
    std::shared_ptr<TextureAsset> load_texture_internal(const std::string& path);
    std::shared_ptr<ShaderAsset> load_shader_internal(const std::string& path);
    std::shared_ptr<MaterialAsset> load_material_internal(const std::string& path);
    std::shared_ptr<AudioAsset> load_audio_internal(const std::string& path);
    
    // Resource cleanup
    void destroy_asset(std::shared_ptr<Asset> asset);

    // File type detection
    static std::string get_extension(const std::string& path);

    render::IRenderer* m_renderer = nullptr;
    bool m_hot_reload_enabled = false;
    ReloadCallback m_reload_callback;

    // Asset caches
    std::unordered_map<std::string, std::shared_ptr<MeshAsset>> m_meshes;
    std::unordered_map<std::string, std::shared_ptr<TextureAsset>> m_textures;
    std::unordered_map<std::string, std::shared_ptr<ShaderAsset>> m_shaders;
    std::unordered_map<std::string, std::shared_ptr<MaterialAsset>> m_materials;
    std::unordered_map<std::string, std::shared_ptr<AudioAsset>> m_audio;
    std::unordered_map<std::string, std::shared_ptr<SceneAsset>> m_scenes;
    std::unordered_map<std::string, std::shared_ptr<PrefabAsset>> m_prefabs;
    
    // Orphans (replaced assets that might still be in use)
    std::vector<std::shared_ptr<Asset>> m_orphans;

    // Loading status tracking
    std::unordered_map<std::string, AssetStatus> m_status;

    // Thread safety
    mutable std::shared_mutex m_mutex;
    std::condition_variable_any m_load_cv;
};

// Global asset manager instance
AssetManager& get_asset_manager();

} // namespace engine::asset
