#include <engine/asset/importer.hpp>
#include <engine/asset/manager.hpp>
#include <algorithm>
#include <cctype>

namespace engine::asset {

static std::string get_extension(const std::string& path) {
    size_t pos = path.rfind('.');
    if (pos == std::string::npos) return "";

    std::string ext = path.substr(pos);
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

std::unique_ptr<Asset> Importer::import(const std::string& path) {
    // Use the global asset manager to load, then clone to unique_ptr
    // Note: This bypasses caching - each call creates a new asset
    auto& mgr = get_asset_manager();

    std::string type = get_asset_type(path);

    if (type == "mesh") {
        auto shared = mgr.load_mesh(path);
        if (shared) {
            auto asset = std::make_unique<MeshAsset>(*shared);
            return asset;
        }
    } else if (type == "texture") {
        auto shared = mgr.load_texture(path);
        if (shared) {
            auto asset = std::make_unique<TextureAsset>(*shared);
            return asset;
        }
    } else if (type == "audio") {
        auto shared = mgr.load_audio(path);
        if (shared) {
            auto asset = std::make_unique<AudioAsset>(*shared);
            return asset;
        }
    } else if (type == "material") {
        auto shared = mgr.load_material(path);
        if (shared) {
            auto asset = std::make_unique<MaterialAsset>(*shared);
            return asset;
        }
    } else if (type == "shader") {
        auto shared = mgr.load_shader(path);
        if (shared) {
            auto asset = std::make_unique<ShaderAsset>(*shared);
            return asset;
        }
    }

    return nullptr;
}

std::string Importer::get_asset_type(const std::string& path) {
    std::string ext = get_extension(path);

    // Mesh formats
    if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb") {
        return "mesh";
    }

    // Texture formats
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
        ext == ".tga" || ext == ".bmp" || ext == ".hdr") {
        return "texture";
    }

    // Audio formats
    if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac") {
        return "audio";
    }

    // Material formats
    if (ext == ".mat" || ext == ".material") {
        return "material";
    }

    // Shader (base path, loads .vs.bin and .fs.bin)
    if (ext == ".shader" || ext.empty()) {
        return "shader";
    }

    return "";
}

} // namespace engine::asset
