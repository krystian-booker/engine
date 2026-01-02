#include <engine/asset/manager.hpp>
#include <engine/asset/hot_reload.hpp>
#include <engine/asset/gltf_importer.hpp>
#include <engine/asset/obj_importer.hpp>
#include <engine/asset/fbx_importer.hpp>
#include <engine/asset/audio_loader.hpp>
#include <engine/asset/material_loader.hpp>
#include <engine/asset/scene_loader.hpp>
#include <engine/asset/prefab_loader.hpp>
#include <engine/asset/dds_loader.hpp>
#include <engine/asset/ktx_loader.hpp>
#include <engine/core/filesystem.hpp>
#include <engine/core/log.hpp>
#include <engine/core/job_system.hpp>
#include <algorithm>
#include <mutex>
#include <cstring>

// STB image implementation
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace engine::asset {

using namespace engine::core;

AssetManager::AssetManager() = default;
AssetManager::~AssetManager() {
    shutdown();
}

void AssetManager::init(render::IRenderer* renderer) {
    if (!renderer) {
        log(LogLevel::Error, "AssetManager::init called with null renderer");
        return;
    }
    std::unique_lock lock(m_mutex);
    m_renderer = renderer;
}

void AssetManager::shutdown() {
    unload_all();
    
    std::unique_lock lock(m_mutex);
    m_renderer = nullptr;
}

// Internal helper to destroy GPU resources
void AssetManager::destroy_asset(std::shared_ptr<Asset> asset) {
    if (!asset || !m_renderer) return;

    if (auto mesh = std::dynamic_pointer_cast<MeshAsset>(asset)) {
        if (mesh->handle.valid()) {
            m_renderer->destroy_mesh(mesh->handle);
            mesh->handle = {UINT32_MAX};
        }
    } 
    else if (auto tex = std::dynamic_pointer_cast<TextureAsset>(asset)) {
        if (tex->handle.valid()) {
            m_renderer->destroy_texture(tex->handle);
            tex->handle = {UINT32_MAX};
        }
    }
    else if (auto shader = std::dynamic_pointer_cast<ShaderAsset>(asset)) {
        if (shader->handle.valid()) {
            m_renderer->destroy_shader(shader->handle);
            shader->handle = {UINT32_MAX};
        }
    }
    else if (auto mat = std::dynamic_pointer_cast<MaterialAsset>(asset)) {
        if (mat->handle.valid()) {
            m_renderer->destroy_material(mat->handle);
            mat->handle = {UINT32_MAX};
        }
    }
    // Audio assets don't have GPU resources to destroy
}

std::shared_ptr<MeshAsset> AssetManager::load_mesh(const std::string& path) {
    // Check cache
    {
        std::shared_lock lock(m_mutex);
        auto it = m_meshes.find(path);
        if (it != m_meshes.end()) return it->second;
    }

    // Check loading status
    {
        std::unique_lock lock(m_mutex);
        if (m_meshes.count(path)) return m_meshes[path];

        if (m_status[path] == AssetStatus::Loading) {
            m_load_cv.wait(lock, [this, &path] {
                return m_status[path] != AssetStatus::Loading;
            });
            if (m_meshes.count(path)) return m_meshes[path];
            return nullptr;
        }
        m_status[path] = AssetStatus::Loading;
    }

    // Load
    auto asset = load_mesh_internal(path);

    // Update
    {
        std::unique_lock lock(m_mutex);
        if (asset) {
            m_meshes[path] = asset;
            m_status[path] = AssetStatus::Loaded;

            if (m_hot_reload_enabled) {
                HotReload::watch(path, [this](const std::string& p) {
                    auto new_asset = load_mesh_internal(p);
                    if (new_asset) {
                        ReloadCallback cb;
                        {
                            std::unique_lock reload_lock(m_mutex);
                            if (m_meshes.count(p)) m_orphans.push_back(m_meshes[p]);
                            m_meshes[p] = new_asset;
                            cb = m_reload_callback;
                        }
                        if (cb) cb(p);
                    }
                });
            }
        } else {
            m_status[path] = AssetStatus::Failed;
        }
        m_load_cv.notify_all();
    }

    return asset;
}

std::shared_ptr<TextureAsset> AssetManager::load_texture(const std::string& path) {
    {
        std::shared_lock lock(m_mutex);
        auto it = m_textures.find(path);
        if (it != m_textures.end()) return it->second;
    }
    {
        std::unique_lock lock(m_mutex);
        if (m_textures.count(path)) return m_textures[path];
        
        if (m_status[path] == AssetStatus::Loading) {
            m_load_cv.wait(lock, [this, &path] {
                return m_status[path] != AssetStatus::Loading;
            });
            if (m_textures.count(path)) return m_textures[path];
            return nullptr;
        }
        m_status[path] = AssetStatus::Loading;
    }

    auto asset = load_texture_internal(path);

    {
        std::unique_lock lock(m_mutex);
        if (asset) {
            m_textures[path] = asset;
            m_status[path] = AssetStatus::Loaded;
            if (m_hot_reload_enabled) {
                HotReload::watch(path, [this](const std::string& p) {
                    auto new_asset = load_texture_internal(p);
                    if (new_asset) {
                        ReloadCallback cb;
                        {
                            std::unique_lock reload_lock(m_mutex);
                            if (m_textures.count(p)) m_orphans.push_back(m_textures[p]);
                            m_textures[p] = new_asset;
                            cb = m_reload_callback;
                        }
                        if (cb) cb(p);
                    }
                });
            }
        } else {
            m_status[path] = AssetStatus::Failed;
        }
        m_load_cv.notify_all();
    }
    return asset;
}

std::shared_ptr<ShaderAsset> AssetManager::load_shader(const std::string& path) {
    {
        std::shared_lock lock(m_mutex);
        auto it = m_shaders.find(path);
        if (it != m_shaders.end()) return it->second;
    }
    {
        std::unique_lock lock(m_mutex);
        if (m_shaders.count(path)) return m_shaders[path];
        
        if (m_status[path] == AssetStatus::Loading) {
            m_load_cv.wait(lock, [this, &path] {
                return m_status[path] != AssetStatus::Loading;
            });
            if (m_shaders.count(path)) return m_shaders[path];
            return nullptr;
        }
        m_status[path] = AssetStatus::Loading;
    }

    auto asset = load_shader_internal(path);

    {
        std::unique_lock lock(m_mutex);
        if (asset) {
            m_shaders[path] = asset;
            m_status[path] = AssetStatus::Loaded;
            
            if (m_hot_reload_enabled) {
                HotReload::watch(path, [this](const std::string& p) {
                    auto new_asset = load_shader_internal(p);
                    if (new_asset) {
                        ReloadCallback cb;
                        {
                            std::unique_lock reload_lock(m_mutex);
                            if (m_shaders.count(p)) m_orphans.push_back(m_shaders[p]);
                            m_shaders[p] = new_asset;
                            cb = m_reload_callback;
                        }
                        if (cb) cb(p);
                    }
                });
            }
        } else {
            m_status[path] = AssetStatus::Failed;
        }
        m_load_cv.notify_all();
    }
    return asset;
}

std::shared_ptr<MaterialAsset> AssetManager::load_material(const std::string& path) {
    {
        std::shared_lock lock(m_mutex);
        auto it = m_materials.find(path);
        if (it != m_materials.end()) return it->second;
    }
    {
        std::unique_lock lock(m_mutex);
        if (m_materials.count(path)) return m_materials[path];
        
        if (m_status[path] == AssetStatus::Loading) {
            m_load_cv.wait(lock, [this, &path] {
                return m_status[path] != AssetStatus::Loading;
            });
            if (m_materials.count(path)) return m_materials[path];
            return nullptr;
        }
        m_status[path] = AssetStatus::Loading;
    }

    auto asset = load_material_internal(path);

    {
        std::unique_lock lock(m_mutex);
        if (asset) {
            m_materials[path] = asset;
            m_status[path] = AssetStatus::Loaded;
            if (m_hot_reload_enabled) {
                HotReload::watch(path, [this](const std::string& p) {
                    auto new_asset = load_material_internal(p);
                    if (new_asset) {
                        ReloadCallback cb;
                        {
                            std::unique_lock reload_lock(m_mutex);
                            if (m_materials.count(p)) m_orphans.push_back(m_materials[p]);
                            m_materials[p] = new_asset;
                            cb = m_reload_callback;
                        }
                        if (cb) cb(p);
                    }
                });
            }
        } else {
            m_status[path] = AssetStatus::Failed;
        }
        m_load_cv.notify_all();
    }
    return asset;
}

std::shared_ptr<AudioAsset> AssetManager::load_audio(const std::string& path) {
    {
        std::shared_lock lock(m_mutex);
        auto it = m_audio.find(path);
        if (it != m_audio.end()) return it->second;
    }
    {
        std::unique_lock lock(m_mutex);
        if (m_audio.count(path)) return m_audio[path];
        
        if (m_status[path] == AssetStatus::Loading) {
            m_load_cv.wait(lock, [this, &path] {
                return m_status[path] != AssetStatus::Loading;
            });
            if (m_audio.count(path)) return m_audio[path];
            return nullptr;
        }
        m_status[path] = AssetStatus::Loading;
    }

    auto asset = load_audio_internal(path);

    {
        std::unique_lock lock(m_mutex);
        if (asset) {
            m_audio[path] = asset;
            m_status[path] = AssetStatus::Loaded;
        } else {
            m_status[path] = AssetStatus::Failed;
        }
        m_load_cv.notify_all();
    }
    return asset;
}

std::shared_ptr<SceneAsset> AssetManager::load_scene(const std::string& path) {
    {
        std::shared_lock lock(m_mutex);
        auto it = m_scenes.find(path);
        if (it != m_scenes.end()) return it->second;
    }
    {
        std::unique_lock lock(m_mutex);
        if (m_scenes.count(path)) return m_scenes[path];
        
        if (m_status[path] == AssetStatus::Loading) {
            m_load_cv.wait(lock, [this, &path] {
                return m_status[path] != AssetStatus::Loading;
            });
            if (m_scenes.count(path)) return m_scenes[path];
            return nullptr;
        }
        m_status[path] = AssetStatus::Loading;
    }

    auto asset = SceneLoader::load(path);

    {
        std::unique_lock lock(m_mutex);
        if (asset) {
            m_scenes[path] = asset;
            m_status[path] = AssetStatus::Loaded;

            if (m_hot_reload_enabled) {
                HotReload::watch(path, [this](const std::string& p) {
                    auto new_asset = SceneLoader::load(p);
                    if (new_asset) {
                        ReloadCallback cb;
                        {
                            std::unique_lock reload_lock(m_mutex);
                            if (m_scenes.count(p)) m_orphans.push_back(m_scenes[p]);
                            m_scenes[p] = new_asset;
                            cb = m_reload_callback;
                        }
                        if (cb) cb(p);
                    }
                });
            }
        } else {
            m_status[path] = AssetStatus::Failed;
        }
        m_load_cv.notify_all();
    }
    return asset;
}

std::shared_ptr<PrefabAsset> AssetManager::load_prefab(const std::string& path) {
    {
        std::shared_lock lock(m_mutex);
        auto it = m_prefabs.find(path);
        if (it != m_prefabs.end()) return it->second;
    }
    {
        std::unique_lock lock(m_mutex);
        if (m_prefabs.count(path)) return m_prefabs[path];
        
        if (m_status[path] == AssetStatus::Loading) {
            m_load_cv.wait(lock, [this, &path] {
                return m_status[path] != AssetStatus::Loading;
            });
            if (m_prefabs.count(path)) return m_prefabs[path];
            return nullptr;
        }
        m_status[path] = AssetStatus::Loading;
    }

    auto asset = PrefabLoader::load(path);

    {
        std::unique_lock lock(m_mutex);
        if (asset) {
            m_prefabs[path] = asset;
            m_status[path] = AssetStatus::Loaded;

            if (m_hot_reload_enabled) {
                HotReload::watch(path, [this](const std::string& p) {
                    auto new_asset = PrefabLoader::load(p);
                    if (new_asset) {
                        ReloadCallback cb;
                        {
                            std::unique_lock reload_lock(m_mutex);
                            if (m_prefabs.count(p)) m_orphans.push_back(m_prefabs[p]);
                            m_prefabs[p] = new_asset;
                            cb = m_reload_callback;
                        }
                        if (cb) cb(p);
                    }
                });
            }
        } else {
            m_status[path] = AssetStatus::Failed;
        }
        m_load_cv.notify_all();
    }
    return asset;
}

std::future<std::shared_ptr<MeshAsset>> AssetManager::load_mesh_async(const std::string& path) {
    return JobSystem::submit_with_result([this, path]() {
        return load_mesh(path);
    });
}

std::future<std::shared_ptr<TextureAsset>> AssetManager::load_texture_async(const std::string& path) {
    return JobSystem::submit_with_result([this, path]() {
        return load_texture(path);
    });
}

std::future<std::shared_ptr<ShaderAsset>> AssetManager::load_shader_async(const std::string& path) {
    return JobSystem::submit_with_result([this, path]() {
        return load_shader(path);
    });
}

std::future<std::shared_ptr<MaterialAsset>> AssetManager::load_material_async(const std::string& path) {
    return JobSystem::submit_with_result([this, path]() {
        return load_material(path);
    });
}

std::future<std::shared_ptr<AudioAsset>> AssetManager::load_audio_async(const std::string& path) {
    return JobSystem::submit_with_result([this, path]() {
        return load_audio(path);
    });
}

std::future<std::shared_ptr<SceneAsset>> AssetManager::load_scene_async(const std::string& path) {
    return JobSystem::submit_with_result([this, path]() {
        return load_scene(path);
    });
}

std::future<std::shared_ptr<PrefabAsset>> AssetManager::load_prefab_async(const std::string& path) {
    return JobSystem::submit_with_result([this, path]() {
        return load_prefab(path);
    });
}

std::shared_ptr<Asset> AssetManager::load(const std::string& path) {
    std::string ext = get_extension(path);

    if (ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".fbx") {
        return load_mesh(path);
    } else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".hdr" || ext == ".exr" || ext == ".dds" || ext == ".ktx" || ext == ".ktx2") {
        return load_texture(path);
    } else if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac") {
        return load_audio(path);
    } else if (ext == ".mat" || ext == ".material") {
        return load_material(path);
    } else if (ext == ".scene" || ext == ".level") {
        return load_scene(path);
    } else if (ext == ".prefab" || ext == ".entity") {
        return load_prefab(path);
    }

    log(LogLevel::Warn, ("Unknown asset type: " + path).c_str());
    return nullptr;
}

bool AssetManager::is_loaded(const std::string& path) const {
    std::shared_lock lock(m_mutex);
    auto it = m_status.find(path);
    return it != m_status.end() && it->second == AssetStatus::Loaded;
}

AssetStatus AssetManager::get_status(const std::string& path) const {
    std::shared_lock lock(m_mutex);
    auto it = m_status.find(path);
    return it != m_status.end() ? it->second : AssetStatus::NotLoaded;
}

void AssetManager::enable_hot_reload(bool enabled) {
    std::unique_lock lock(m_mutex);
    m_hot_reload_enabled = enabled;
    if (enabled) {
        HotReload::init();
    } else {
        HotReload::shutdown();
    }
}

void AssetManager::poll_hot_reload() {
    bool enabled;
    {
        std::shared_lock lock(m_mutex);
        enabled = m_hot_reload_enabled;
    }
    
    if (enabled) {
        HotReload::poll();
    }
}

void AssetManager::unload(const std::string& path) {
    std::unique_lock lock(m_mutex);

    if (m_hot_reload_enabled) {
        HotReload::unwatch(path);
    }

    auto remove_from = [&](auto& map) {
        auto it = map.find(path);
        if (it != map.end()) {
            destroy_asset(it->second);
            map.erase(it);
        }
    };

    remove_from(m_meshes);
    remove_from(m_textures);
    remove_from(m_shaders);
    remove_from(m_materials);
    remove_from(m_audio);
    remove_from(m_scenes);
    remove_from(m_prefabs);

    m_status.erase(path);
    m_load_cv.notify_all();
}

void AssetManager::unload_unused() {
    std::unique_lock lock(m_mutex);

    auto prune = [&](auto& map) {
        for (auto it = map.begin(); it != map.end();) {
            if (it->second.use_count() == 1) {
                destroy_asset(it->second);
                m_status.erase(it->first);
                it = map.erase(it);
            } else {
                ++it;
            }
        }
    };

    prune(m_meshes);
    prune(m_textures);
    prune(m_shaders);
    prune(m_materials);
    prune(m_audio);
    prune(m_scenes);
    prune(m_prefabs);

    // Prune orphans
    for (auto it = m_orphans.begin(); it != m_orphans.end();) {
        if (it->use_count() == 1) {
            destroy_asset(*it);
            it = m_orphans.erase(it);
        } else {
            ++it;
        }
    }
}

void AssetManager::unload_all() {
    std::unique_lock lock(m_mutex);

    if (m_hot_reload_enabled) {
        HotReload::shutdown();
    }

    for (auto& [k, v] : m_meshes) destroy_asset(v);
    for (auto& [k, v] : m_textures) destroy_asset(v);
    for (auto& [k, v] : m_shaders) destroy_asset(v);
    for (auto& [k, v] : m_materials) destroy_asset(v);

    m_meshes.clear();
    m_textures.clear();
    m_shaders.clear();
    m_materials.clear();
    m_audio.clear();
    m_scenes.clear();
    m_prefabs.clear();
    
    for (auto& asset : m_orphans) destroy_asset(asset);
    m_orphans.clear();

    m_status.clear();
}

size_t AssetManager::get_loaded_count() const {
    std::shared_lock lock(m_mutex);
    return m_meshes.size() + m_textures.size() + m_shaders.size() +
           m_materials.size() + m_audio.size() + m_scenes.size() + m_prefabs.size();
}

size_t AssetManager::get_memory_usage() const {
    std::shared_lock lock(m_mutex);
    size_t total = 0;

    // Mesh memory: vertices + indices (CPU-side estimate, GPU memory is separate)
    for (const auto& [path, mesh] : m_meshes) {
        if (mesh) {
            // Vertex size: position(12) + normal(12) + texcoord(8) + color(16) + tangent(12) = 60 bytes
            total += mesh->vertex_count * 60;
            total += mesh->index_count * sizeof(uint32_t);
        }
    }

    // Texture memory: width * height * 4 bytes (RGBA8)
    for (const auto& [path, tex] : m_textures) {
        if (tex) {
            total += tex->width * tex->height * 4;
        }
    }

    // Audio memory: raw PCM data
    for (const auto& [path, audio] : m_audio) {
        if (audio) {
            total += audio->data.size();
        }
    }

    // Shaders and materials are small, add fixed estimate per instance
    total += m_shaders.size() * 1024;     // ~1KB per shader program
    total += m_materials.size() * 256;    // ~256B per material

    return total;
}

void AssetManager::set_reload_callback(ReloadCallback callback) {
    std::unique_lock lock(m_mutex);
    m_reload_callback = std::move(callback);
}

// Internal loading implementations
std::shared_ptr<MeshAsset> AssetManager::load_mesh_internal(const std::string& path) {
    if (!m_renderer) return nullptr;
    
    log(LogLevel::Debug, ("Loading mesh: " + path).c_str());

    std::string ext = get_extension(path);

    // Use glTF importer for glTF/glB formats
    if (ext == ".gltf" || ext == ".glb") {
        return GltfImporter::import_mesh(path, m_renderer);
    }

    // Use OBJ importer for Wavefront OBJ format
    if (ext == ".obj") {
        return ObjImporter::import_mesh(path, m_renderer);
    }

    // Use FBX importer for Autodesk FBX format
    if (ext == ".fbx") {
        return FbxImporter::import_mesh(path, m_renderer);
    }

    log(LogLevel::Error, ("Unknown mesh format: " + path).c_str());
    return nullptr;
}

// Helper to calculate mip levels for a texture
static uint32_t calculate_mip_levels(uint32_t width, uint32_t height) {
    uint32_t levels = 1;
    while (width > 1 || height > 1) {
        width = std::max(1u, width / 2);
        height = std::max(1u, height / 2);
        levels++;
    }
    return levels;
}

// Helper to generate mipmaps for RGBA8 texture
static void generate_mipmaps_rgba8(
    const std::vector<uint8_t>& src,
    uint32_t width, uint32_t height,
    std::vector<uint8_t>& out_data,
    uint32_t& out_mip_levels)
{
    out_mip_levels = calculate_mip_levels(width, height);

    // Start with base level
    out_data = src;

    uint32_t mip_width = width;
    uint32_t mip_height = height;
    size_t src_offset = 0;

    for (uint32_t level = 1; level < out_mip_levels; level++) {
        uint32_t prev_width = mip_width;
        uint32_t prev_height = mip_height;
        mip_width = std::max(1u, mip_width / 2);
        mip_height = std::max(1u, mip_height / 2);

        size_t mip_size = static_cast<size_t>(mip_width) * mip_height * 4;
        size_t prev_offset = src_offset;
        src_offset = out_data.size();
        out_data.resize(out_data.size() + mip_size);

        // Box filter downsampling
        for (uint32_t y = 0; y < mip_height; y++) {
            for (uint32_t x = 0; x < mip_width; x++) {
                uint32_t src_x = x * 2;
                uint32_t src_y = y * 2;

                // Sample 2x2 texels
                uint32_t r = 0, g = 0, b = 0, a = 0;
                for (uint32_t dy = 0; dy < 2 && (src_y + dy) < prev_height; dy++) {
                    for (uint32_t dx = 0; dx < 2 && (src_x + dx) < prev_width; dx++) {
                        size_t idx = prev_offset + ((src_y + dy) * prev_width + (src_x + dx)) * 4;
                        r += out_data[idx + 0];
                        g += out_data[idx + 1];
                        b += out_data[idx + 2];
                        a += out_data[idx + 3];
                    }
                }

                size_t dst_idx = src_offset + (y * mip_width + x) * 4;
                out_data[dst_idx + 0] = static_cast<uint8_t>(r / 4);
                out_data[dst_idx + 1] = static_cast<uint8_t>(g / 4);
                out_data[dst_idx + 2] = static_cast<uint8_t>(b / 4);
                out_data[dst_idx + 3] = static_cast<uint8_t>(a / 4);
            }
        }
    }
}

// Helper to generate mipmaps for RGBA16F texture (HDR)
static void generate_mipmaps_rgba16f(
    const float* src,
    uint32_t width, uint32_t height,
    std::vector<uint8_t>& out_data,
    uint32_t& out_mip_levels)
{
    out_mip_levels = calculate_mip_levels(width, height);

    // Convert to half-float storage
    size_t base_size = static_cast<size_t>(width) * height * 4 * sizeof(uint16_t);
    out_data.resize(base_size);

    // Convert float to half-float for base level
    auto float_to_half = [](float f) -> uint16_t {
        uint32_t bits = *reinterpret_cast<uint32_t*>(&f);
        uint32_t sign = (bits >> 31) & 0x1;
        int32_t exp = ((bits >> 23) & 0xFF) - 127;
        uint32_t mantissa = bits & 0x7FFFFF;

        if (exp > 15) {
            // Overflow to infinity
            return static_cast<uint16_t>((sign << 15) | 0x7C00);
        } else if (exp < -14) {
            // Underflow to zero
            return static_cast<uint16_t>(sign << 15);
        } else {
            return static_cast<uint16_t>((sign << 15) | ((exp + 15) << 10) | (mantissa >> 13));
        }
    };

    uint16_t* dst = reinterpret_cast<uint16_t*>(out_data.data());
    for (size_t i = 0; i < static_cast<size_t>(width) * height * 4; i++) {
        dst[i] = float_to_half(src[i]);
    }

    // Generate mip chain (simplified - stores as half-float)
    uint32_t mip_width = width;
    uint32_t mip_height = height;

    // For HDR, we store all mip levels as half-float
    std::vector<float> prev_level(width * height * 4);
    std::memcpy(prev_level.data(), src, width * height * 4 * sizeof(float));

    for (uint32_t level = 1; level < out_mip_levels; level++) {
        uint32_t prev_width = mip_width;
        uint32_t prev_height = mip_height;
        mip_width = std::max(1u, mip_width / 2);
        mip_height = std::max(1u, mip_height / 2);

        std::vector<float> mip_level(mip_width * mip_height * 4);

        // Box filter downsampling
        for (uint32_t y = 0; y < mip_height; y++) {
            for (uint32_t x = 0; x < mip_width; x++) {
                uint32_t src_x = x * 2;
                uint32_t src_y = y * 2;

                float r = 0, g = 0, b = 0, a = 0;
                int count = 0;
                for (uint32_t dy = 0; dy < 2 && (src_y + dy) < prev_height; dy++) {
                    for (uint32_t dx = 0; dx < 2 && (src_x + dx) < prev_width; dx++) {
                        size_t idx = ((src_y + dy) * prev_width + (src_x + dx)) * 4;
                        r += prev_level[idx + 0];
                        g += prev_level[idx + 1];
                        b += prev_level[idx + 2];
                        a += prev_level[idx + 3];
                        count++;
                    }
                }

                size_t dst_idx = (y * mip_width + x) * 4;
                mip_level[dst_idx + 0] = r / count;
                mip_level[dst_idx + 1] = g / count;
                mip_level[dst_idx + 2] = b / count;
                mip_level[dst_idx + 3] = a / count;
            }
        }

        // Convert and append to output
        size_t offset = out_data.size();
        size_t mip_size = static_cast<size_t>(mip_width) * mip_height * 4 * sizeof(uint16_t);
        out_data.resize(out_data.size() + mip_size);

        dst = reinterpret_cast<uint16_t*>(out_data.data() + offset);
        for (size_t i = 0; i < static_cast<size_t>(mip_width) * mip_height * 4; i++) {
            dst[i] = float_to_half(mip_level[i]);
        }

        prev_level = std::move(mip_level);
    }
}

std::shared_ptr<TextureAsset> AssetManager::load_texture_internal(const std::string& path) {
    if (!m_renderer) return nullptr;

    std::string ext = get_extension(path);

    // Handle DDS format
    if (ext == ".dds") {
        DDSLoader::DDSData dds_data;
        if (!DDSLoader::load(path, dds_data)) {
            log(LogLevel::Error, ("Failed to load DDS texture: " + path).c_str());
            return nullptr;
        }

        auto asset = std::make_shared<TextureAsset>();
        asset->path = path;
        asset->width = dds_data.width;
        asset->height = dds_data.height;
        asset->mip_levels = dds_data.mip_levels;
        asset->is_hdr = (dds_data.format == render::TextureFormat::RGBA16F ||
                         dds_data.format == render::TextureFormat::RGBA32F);

        render::TextureData tex_data;
        tex_data.width = dds_data.width;
        tex_data.height = dds_data.height;
        tex_data.depth = dds_data.depth;
        tex_data.mip_levels = dds_data.mip_levels;
        tex_data.format = dds_data.format;
        tex_data.is_cubemap = dds_data.is_cubemap;
        tex_data.pixels = std::move(dds_data.data);

        asset->handle = m_renderer->create_texture(tex_data);
        return asset;
    }

    // Handle KTX format
    if (ext == ".ktx" || ext == ".ktx2") {
        KTXLoader::KTXData ktx_data;
        if (!KTXLoader::load(path, ktx_data)) {
            log(LogLevel::Error, ("Failed to load KTX texture: " + path).c_str());
            return nullptr;
        }

        auto asset = std::make_shared<TextureAsset>();
        asset->path = path;
        asset->width = ktx_data.width;
        asset->height = ktx_data.height;
        asset->mip_levels = ktx_data.mip_levels;
        asset->is_hdr = (ktx_data.format == render::TextureFormat::RGBA16F ||
                         ktx_data.format == render::TextureFormat::RGBA32F);

        render::TextureData tex_data;
        tex_data.width = ktx_data.width;
        tex_data.height = ktx_data.height;
        tex_data.depth = ktx_data.depth;
        tex_data.mip_levels = ktx_data.mip_levels;
        tex_data.format = ktx_data.format;
        tex_data.is_cubemap = ktx_data.is_cubemap;
        tex_data.pixels = std::move(ktx_data.data);

        asset->handle = m_renderer->create_texture(tex_data);
        return asset;
    }

    bool is_hdr = (ext == ".hdr" || ext == ".exr");

    auto asset = std::make_shared<TextureAsset>();
    asset->path = path;
    asset->is_hdr = is_hdr;

    render::TextureData tex_data;

    if (is_hdr) {
        // Load HDR image
        int width, height, channels;
        float* data = stbi_loadf(path.c_str(), &width, &height, &channels, 4);

        if (!data) {
            log(LogLevel::Error, ("Failed to load HDR texture: " + path).c_str());
            return nullptr;
        }

        asset->width = static_cast<uint32_t>(width);
        asset->height = static_cast<uint32_t>(height);
        asset->channels = static_cast<uint32_t>(channels);
        asset->has_alpha = channels == 4;

        tex_data.width = asset->width;
        tex_data.height = asset->height;
        tex_data.format = render::TextureFormat::RGBA16F;

        // Generate mipmaps for HDR
        generate_mipmaps_rgba16f(data, asset->width, asset->height, tex_data.pixels, tex_data.mip_levels);
        asset->mip_levels = tex_data.mip_levels;

        stbi_image_free(data);

        log(LogLevel::Debug, ("Loaded HDR texture: " + path + " (" +
            std::to_string(asset->width) + "x" + std::to_string(asset->height) +
            ", " + std::to_string(asset->mip_levels) + " mips)").c_str());
    } else {
        // Load LDR image
        int width, height, channels;
        unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);

        if (!data) {
            log(LogLevel::Error, ("Failed to load texture: " + path).c_str());
            return nullptr;
        }

        asset->width = static_cast<uint32_t>(width);
        asset->height = static_cast<uint32_t>(height);
        asset->channels = static_cast<uint32_t>(channels);
        asset->has_alpha = channels == 4;

        tex_data.width = asset->width;
        tex_data.height = asset->height;
        tex_data.format = render::TextureFormat::RGBA8;

        // Generate mipmaps
        std::vector<uint8_t> base_pixels(data, data + width * height * 4);
        generate_mipmaps_rgba8(base_pixels, asset->width, asset->height, tex_data.pixels, tex_data.mip_levels);
        asset->mip_levels = tex_data.mip_levels;

        stbi_image_free(data);

        log(LogLevel::Debug, ("Loaded texture: " + path + " (" +
            std::to_string(asset->width) + "x" + std::to_string(asset->height) +
            ", " + std::to_string(asset->mip_levels) + " mips)").c_str());
    }

    asset->handle = m_renderer->create_texture(tex_data);

    return asset;
}

std::shared_ptr<ShaderAsset> AssetManager::load_shader_internal(const std::string& path) {
    if (!m_renderer) {
        log(LogLevel::Error, "Cannot load shader: renderer not initialized");
        return nullptr;
    }

    // Shader path convention: "shaders/pbr" loads:
    //   - shaders/pbr.vs.bin (vertex shader)
    //   - shaders/pbr.fs.bin (fragment shader)
    std::string vs_path = path + ".vs.bin";
    std::string fs_path = path + ".fs.bin";

    auto vs_binary = FileSystem::read_binary(vs_path);
    if (vs_binary.empty()) {
        log(LogLevel::Error, ("Failed to load vertex shader: " + vs_path).c_str());
        return nullptr;
    }

    auto fs_binary = FileSystem::read_binary(fs_path);
    if (fs_binary.empty()) {
        log(LogLevel::Error, ("Failed to load fragment shader: " + fs_path).c_str());
        return nullptr;
    }

    render::ShaderData shader_data;
    shader_data.vertex_binary = std::move(vs_binary);
    shader_data.fragment_binary = std::move(fs_binary);

    auto asset = std::make_shared<ShaderAsset>();
    asset->path = path;
    asset->handle = m_renderer->create_shader(shader_data);

    if (!asset->handle.valid()) {
        log(LogLevel::Error, ("Failed to create shader program: " + path).c_str());
        return nullptr;
    }

    log(LogLevel::Debug, ("Loaded shader: " + path).c_str());
    return asset;
}

std::shared_ptr<MaterialAsset> AssetManager::load_material_internal(const std::string& path) {
    std::string ext = get_extension(path);

    // Check for glTF material reference (path#material0 format)
    size_t hash_pos = path.find('#');
    if (hash_pos != std::string::npos) {
        std::string gltf_path = path.substr(0, hash_pos);
        std::string suffix = path.substr(hash_pos + 1);

        // Parse material index from suffix like "material0"
        if (suffix.rfind("material", 0) == 0) {
            uint32_t mat_index = static_cast<uint32_t>(std::stoul(suffix.substr(8)));
            return MaterialLoader::load_from_gltf(gltf_path, mat_index, *this, m_renderer);
        }
    }

    // JSON-based material file
    if (ext == ".mat" || ext == ".material" || ext == ".json") {
        return MaterialLoader::load_from_json(path, *this, m_renderer);
    }

    log(LogLevel::Error, ("Unknown material format: " + path).c_str());
    return nullptr;
}

std::shared_ptr<AudioAsset> AssetManager::load_audio_internal(const std::string& path) {
    std::vector<uint8_t> pcm_data;
    AudioFormat format;

    if (!AudioLoader::load(path, pcm_data, format)) {
        log(LogLevel::Error, ("Failed to load audio: " + path + " - " + AudioLoader::get_last_error()).c_str());
        return nullptr;
    }

    auto asset = std::make_shared<AudioAsset>();
    asset->path = path;
    asset->data = std::move(pcm_data);
    asset->sample_rate = format.sample_rate;
    asset->channels = format.channels;
    asset->sample_count = static_cast<uint32_t>(format.total_frames);

    log(LogLevel::Debug, ("Loaded audio: " + path).c_str());
    return asset;
}

std::string AssetManager::get_extension(const std::string& path) {
    size_t pos = path.rfind('.');
    if (pos == std::string::npos) return "";

    std::string ext = path.substr(pos);
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

// Global instance
static AssetManager s_asset_manager;

AssetManager& get_asset_manager() {
    return s_asset_manager;
}

} // namespace engine::asset
