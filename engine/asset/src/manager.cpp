#include <engine/asset/manager.hpp>
#include <engine/asset/hot_reload.hpp>
#include <engine/asset/gltf_importer.hpp>
#include <engine/core/filesystem.hpp>
#include <engine/core/log.hpp>
#include <engine/core/job_system.hpp>
#include <algorithm>

// STB image implementation
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace engine::asset {

using namespace engine::core;

AssetManager::AssetManager() = default;
AssetManager::~AssetManager() = default;

void AssetManager::init(render::IRenderer* renderer) {
    m_renderer = renderer;
}

void AssetManager::shutdown() {
    unload_all();
    m_renderer = nullptr;
}

std::shared_ptr<MeshAsset> AssetManager::load_mesh(const std::string& path) {
    // Check cache first
    auto it = m_meshes.find(path);
    if (it != m_meshes.end()) {
        return it->second;
    }

    m_status[path] = AssetStatus::Loading;
    auto asset = load_mesh_internal(path);

    if (asset) {
        m_meshes[path] = asset;
        m_status[path] = AssetStatus::Loaded;

        if (m_hot_reload_enabled) {
            HotReload::watch(path, [this](const std::string& p) {
                // Reload on change
                auto new_asset = load_mesh_internal(p);
                if (new_asset) {
                    m_meshes[p] = new_asset;
                    if (m_reload_callback) m_reload_callback(p);
                }
            });
        }
    } else {
        m_status[path] = AssetStatus::Failed;
    }

    return asset;
}

std::shared_ptr<TextureAsset> AssetManager::load_texture(const std::string& path) {
    // Check cache first
    auto it = m_textures.find(path);
    if (it != m_textures.end()) {
        return it->second;
    }

    m_status[path] = AssetStatus::Loading;
    auto asset = load_texture_internal(path);

    if (asset) {
        m_textures[path] = asset;
        m_status[path] = AssetStatus::Loaded;

        if (m_hot_reload_enabled) {
            HotReload::watch(path, [this](const std::string& p) {
                auto new_asset = load_texture_internal(p);
                if (new_asset) {
                    m_textures[p] = new_asset;
                    if (m_reload_callback) m_reload_callback(p);
                }
            });
        }
    } else {
        m_status[path] = AssetStatus::Failed;
    }

    return asset;
}

std::shared_ptr<ShaderAsset> AssetManager::load_shader(const std::string& path) {
    auto it = m_shaders.find(path);
    if (it != m_shaders.end()) {
        return it->second;
    }

    m_status[path] = AssetStatus::Loading;
    auto asset = load_shader_internal(path);

    if (asset) {
        m_shaders[path] = asset;
        m_status[path] = AssetStatus::Loaded;
    } else {
        m_status[path] = AssetStatus::Failed;
    }

    return asset;
}

std::shared_ptr<MaterialAsset> AssetManager::load_material(const std::string& path) {
    auto it = m_materials.find(path);
    if (it != m_materials.end()) {
        return it->second;
    }

    m_status[path] = AssetStatus::Loading;
    auto asset = load_material_internal(path);

    if (asset) {
        m_materials[path] = asset;
        m_status[path] = AssetStatus::Loaded;
    } else {
        m_status[path] = AssetStatus::Failed;
    }

    return asset;
}

std::shared_ptr<AudioAsset> AssetManager::load_audio(const std::string& path) {
    auto it = m_audio.find(path);
    if (it != m_audio.end()) {
        return it->second;
    }

    m_status[path] = AssetStatus::Loading;
    auto asset = load_audio_internal(path);

    if (asset) {
        m_audio[path] = asset;
        m_status[path] = AssetStatus::Loaded;
    } else {
        m_status[path] = AssetStatus::Failed;
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

std::shared_ptr<Asset> AssetManager::load(const std::string& path) {
    std::string ext = get_extension(path);

    if (ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".fbx") {
        return load_mesh(path);
    } else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp") {
        return load_texture(path);
    } else if (ext == ".wav" || ext == ".ogg" || ext == ".mp3") {
        return load_audio(path);
    } else if (ext == ".mat" || ext == ".material") {
        return load_material(path);
    }

    log(LogLevel::Warn, ("Unknown asset type: " + path).c_str());
    return nullptr;
}

bool AssetManager::is_loaded(const std::string& path) const {
    auto it = m_status.find(path);
    return it != m_status.end() && it->second == AssetStatus::Loaded;
}

AssetStatus AssetManager::get_status(const std::string& path) const {
    auto it = m_status.find(path);
    return it != m_status.end() ? it->second : AssetStatus::NotLoaded;
}

void AssetManager::enable_hot_reload(bool enabled) {
    m_hot_reload_enabled = enabled;
    if (enabled) {
        HotReload::init();
    } else {
        HotReload::shutdown();
    }
}

void AssetManager::poll_hot_reload() {
    if (m_hot_reload_enabled) {
        HotReload::poll();
    }
}

void AssetManager::unload(const std::string& path) {
    if (m_hot_reload_enabled) {
        HotReload::unwatch(path);
    }

    m_meshes.erase(path);
    m_textures.erase(path);
    m_shaders.erase(path);
    m_materials.erase(path);
    m_audio.erase(path);
    m_status.erase(path);
}

void AssetManager::unload_unused() {
    // Remove assets with only one reference (the cache itself)
    for (auto it = m_meshes.begin(); it != m_meshes.end();) {
        if (it->second.use_count() == 1) {
            m_status.erase(it->first);
            it = m_meshes.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_textures.begin(); it != m_textures.end();) {
        if (it->second.use_count() == 1) {
            m_status.erase(it->first);
            it = m_textures.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_shaders.begin(); it != m_shaders.end();) {
        if (it->second.use_count() == 1) {
            m_status.erase(it->first);
            it = m_shaders.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_materials.begin(); it != m_materials.end();) {
        if (it->second.use_count() == 1) {
            m_status.erase(it->first);
            it = m_materials.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_audio.begin(); it != m_audio.end();) {
        if (it->second.use_count() == 1) {
            m_status.erase(it->first);
            it = m_audio.erase(it);
        } else {
            ++it;
        }
    }
}

void AssetManager::unload_all() {
    if (m_hot_reload_enabled) {
        HotReload::shutdown();
    }

    m_meshes.clear();
    m_textures.clear();
    m_shaders.clear();
    m_materials.clear();
    m_audio.clear();
    m_status.clear();
}

size_t AssetManager::get_loaded_count() const {
    return m_meshes.size() + m_textures.size() + m_shaders.size() +
           m_materials.size() + m_audio.size();
}

size_t AssetManager::get_memory_usage() const {
    // Rough estimate - would need more detailed tracking
    return 0;
}

void AssetManager::set_reload_callback(ReloadCallback callback) {
    m_reload_callback = std::move(callback);
}

// Internal loading implementations
std::shared_ptr<MeshAsset> AssetManager::load_mesh_internal(const std::string& path) {
    log(LogLevel::Debug, ("Loading mesh: " + path).c_str());

    std::string ext = get_extension(path);

    // Use glTF importer for supported formats
    if (ext == ".gltf" || ext == ".glb") {
        return GltfImporter::import_mesh(path, m_renderer);
    }

    // TODO: Support for .obj and .fbx formats
    if (ext == ".obj" || ext == ".fbx") {
        log(LogLevel::Warn, ("Unsupported mesh format (only glTF supported): " + path).c_str());
        return nullptr;
    }

    log(LogLevel::Error, ("Unknown mesh format: " + path).c_str());
    return nullptr;
}

std::shared_ptr<TextureAsset> AssetManager::load_texture_internal(const std::string& path) {
    if (!m_renderer) return nullptr;

    int width, height, channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);

    if (!data) {
        log(LogLevel::Error, ("Failed to load texture: " + path).c_str());
        return nullptr;
    }

    auto asset = std::make_shared<TextureAsset>();
    asset->path = path;
    asset->width = static_cast<uint32_t>(width);
    asset->height = static_cast<uint32_t>(height);
    asset->channels = static_cast<uint32_t>(channels);
    asset->has_alpha = channels == 4;

    render::TextureData tex_data;
    tex_data.width = asset->width;
    tex_data.height = asset->height;
    tex_data.format = render::TextureFormat::RGBA8;
    tex_data.pixels.assign(data, data + width * height * 4);

    asset->handle = m_renderer->create_texture(tex_data);

    stbi_image_free(data);

    log(LogLevel::Debug, ("Loaded texture: " + path).c_str());
    return asset;
}

std::shared_ptr<ShaderAsset> AssetManager::load_shader_internal(const std::string& path) {
    // Load compiled shader binary
    auto binary = FileSystem::read_binary(path);
    if (binary.empty()) {
        log(LogLevel::Error, ("Failed to load shader: " + path).c_str());
        return nullptr;
    }

    // TODO: Implement proper shader loading (vs + fs pair)
    return nullptr;
}

std::shared_ptr<MaterialAsset> AssetManager::load_material_internal(const std::string& /*path*/) {
    // TODO: Implement JSON-based material loading
    return nullptr;
}

std::shared_ptr<AudioAsset> AssetManager::load_audio_internal(const std::string& path) {
    auto data = FileSystem::read_binary(path);
    if (data.empty()) {
        log(LogLevel::Error, ("Failed to load audio: " + path).c_str());
        return nullptr;
    }

    auto asset = std::make_shared<AudioAsset>();
    asset->path = path;
    asset->data = std::move(data);
    // TODO: Parse audio format for sample_rate, channels, etc.

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
