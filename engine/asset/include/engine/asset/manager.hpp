#pragma once

#include <engine/asset/types.hpp>
#include <engine/asset/streaming.hpp>
#include <engine/asset/asset_registry.hpp>
#include <engine/core/uuid.hpp>
#include <engine/render/renderer.hpp>
#include <memory>
#include <unordered_map>
#include <string>
#include <future>
#include <functional>
#include <shared_mutex>
#include <condition_variable>
#include <atomic>

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

    // ========================================================================
    // Path-based loading (registers asset if not already registered)
    // ========================================================================
    std::shared_ptr<MeshAsset> load_mesh(const std::string& path);
    std::shared_ptr<TextureAsset> load_texture(const std::string& path);
    std::shared_ptr<ShaderAsset> load_shader(const std::string& path);
    std::shared_ptr<MaterialAsset> load_material(const std::string& path);
    std::shared_ptr<AudioAsset> load_audio(const std::string& path);
    std::shared_ptr<SceneAsset> load_scene(const std::string& path);
    std::shared_ptr<PrefabAsset> load_prefab(const std::string& path);
    std::shared_ptr<AnimationAsset> load_animation(const std::string& path);
    std::shared_ptr<SkeletonAsset> load_skeleton(const std::string& path);
    std::vector<std::shared_ptr<AnimationAsset>> load_animations(const std::string& path);

    // ========================================================================
    // UUID-based loading (requires asset to be registered in AssetRegistry)
    // ========================================================================
    std::shared_ptr<MeshAsset> load_mesh(UUID id);
    std::shared_ptr<TextureAsset> load_texture(UUID id);
    std::shared_ptr<ShaderAsset> load_shader(UUID id);
    std::shared_ptr<MaterialAsset> load_material(UUID id);
    std::shared_ptr<AudioAsset> load_audio(UUID id);
    std::shared_ptr<SceneAsset> load_scene(UUID id);
    std::shared_ptr<PrefabAsset> load_prefab(UUID id);
    std::shared_ptr<AnimationAsset> load_animation(UUID id);
    std::shared_ptr<SkeletonAsset> load_skeleton(UUID id);

    // ========================================================================
    // Asynchronous loading (path-based)
    // ========================================================================
    std::future<std::shared_ptr<MeshAsset>> load_mesh_async(const std::string& path);
    std::future<std::shared_ptr<TextureAsset>> load_texture_async(const std::string& path);
    std::future<std::shared_ptr<ShaderAsset>> load_shader_async(const std::string& path);
    std::future<std::shared_ptr<MaterialAsset>> load_material_async(const std::string& path);
    std::future<std::shared_ptr<AudioAsset>> load_audio_async(const std::string& path);
    std::future<std::shared_ptr<SceneAsset>> load_scene_async(const std::string& path);
    std::future<std::shared_ptr<PrefabAsset>> load_prefab_async(const std::string& path);
    std::future<std::shared_ptr<AnimationAsset>> load_animation_async(const std::string& path);
    std::future<std::vector<std::shared_ptr<AnimationAsset>>> load_animations_async(const std::string& path);
    std::future<std::shared_ptr<SkeletonAsset>> load_skeleton_async(const std::string& path);

    // Generic load by extension
    std::shared_ptr<Asset> load(const std::string& path);

    // Streaming API - for large assets that should be loaded on-demand
    std::unique_ptr<AudioStream> open_audio_stream(const std::string& path);
    std::unique_ptr<TextureStream> open_texture_stream(const std::string& path);

    // ========================================================================
    // Status checking (both path and UUID-based)
    // ========================================================================
    bool is_loaded(const std::string& path) const;
    bool is_loaded(UUID id) const;
    AssetStatus get_status(const std::string& path) const;
    AssetStatus get_status(UUID id) const;

    // Hot reload support
    void enable_hot_reload(bool enabled);
    void poll_hot_reload();

    // ========================================================================
    // Unload assets (both path and UUID-based)
    // ========================================================================
    void unload(const std::string& path);
    void unload(UUID id);
    void unload_unused();  // Unload assets with refcount == 1
    void unload_all();

    // Statistics
    size_t get_loaded_count() const;
    size_t get_memory_usage() const;

    // Reload callback
    using ReloadCallback = std::function<void(UUID id, const std::string& path)>;
    void set_reload_callback(ReloadCallback callback);

private:
    // Internal loading functions (returns asset with UUID assigned)
    std::shared_ptr<MeshAsset> load_mesh_internal(const std::string& path, UUID id);
    std::shared_ptr<TextureAsset> load_texture_internal(const std::string& path, UUID id);
    std::shared_ptr<ShaderAsset> load_shader_internal(const std::string& path, UUID id);
    std::shared_ptr<MaterialAsset> load_material_internal(const std::string& path, UUID id);
    std::shared_ptr<AudioAsset> load_audio_internal(const std::string& path, UUID id);
    std::vector<std::shared_ptr<AnimationAsset>> load_animations_internal(const std::string& path, UUID id);
    std::shared_ptr<SkeletonAsset> load_skeleton_internal(const std::string& path, UUID id);
    
    // Helper to get or register asset in registry
    UUID ensure_registered(const std::string& path, AssetType type);
    
    // Resource cleanup
    void destroy_asset(std::shared_ptr<Asset> asset);
    void cleanup_orphans_if_needed();  // Prune orphan list when threshold exceeded

    // File type detection
    static std::string get_extension(const std::string& path);

    render::IRenderer* m_renderer = nullptr;
    bool m_hot_reload_enabled = false;
    ReloadCallback m_reload_callback;

    // Asset caches (UUID-keyed for stability across file renames)
    std::unordered_map<UUID, std::shared_ptr<MeshAsset>> m_meshes;
    std::unordered_map<UUID, std::shared_ptr<TextureAsset>> m_textures;
    std::unordered_map<UUID, std::shared_ptr<ShaderAsset>> m_shaders;
    std::unordered_map<UUID, std::shared_ptr<MaterialAsset>> m_materials;
    std::unordered_map<UUID, std::shared_ptr<AudioAsset>> m_audio;
    std::unordered_map<UUID, std::shared_ptr<SceneAsset>> m_scenes;
    std::unordered_map<UUID, std::shared_ptr<PrefabAsset>> m_prefabs;
    std::unordered_map<UUID, std::shared_ptr<AnimationAsset>> m_animations;
    std::unordered_map<UUID, std::shared_ptr<SkeletonAsset>> m_skeletons;

    // Animation collection cache (for load_animations which returns multiple)
    std::unordered_map<UUID, std::vector<std::shared_ptr<AnimationAsset>>> m_animation_sets;
    
    // Orphans (replaced assets that might still be in use)
    std::vector<std::shared_ptr<Asset>> m_orphans;

    // Loading status tracking (UUID-keyed)
    std::unordered_map<UUID, AssetStatus> m_status;

    // Thread safety
    mutable std::shared_mutex m_mutex;
    std::condition_variable_any m_load_cv;

    // Lifetime tracking for hot reload callbacks (prevents use-after-free)
    std::shared_ptr<std::atomic<bool>> m_alive = std::make_shared<std::atomic<bool>>(true);
};

// Global asset manager instance
AssetManager& get_asset_manager();

} // namespace engine::asset
