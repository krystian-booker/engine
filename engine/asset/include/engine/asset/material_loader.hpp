#pragma once

#include <engine/asset/types.hpp>
#include <engine/render/renderer.hpp>
#include <string>
#include <memory>

namespace engine::asset {

// Forward declaration
class AssetManager;

// JSON material file schema (.mat)
// {
//   "shader": "shaders/pbr",
//   "textures": {
//     "albedo": "textures/diffuse.png",
//     "normal": "textures/normal.png",
//     "metallic_roughness": "textures/mr.png"
//   },
//   "properties": {
//     "metallic": 0.0,
//     "roughness": 0.5,
//     "base_color": [1.0, 1.0, 1.0, 1.0]
//   },
//   "double_sided": false,
//   "transparent": false
// }

class MaterialLoader {
public:
    // Load from standalone .mat JSON file
    static std::shared_ptr<MaterialAsset> load_from_json(
        const std::string& path,
        AssetManager& asset_manager,
        render::IRenderer* renderer);

    // Extract material from glTF model at specified index
    static std::shared_ptr<MaterialAsset> load_from_gltf(
        const std::string& gltf_path,
        uint32_t material_index,
        AssetManager& asset_manager,
        render::IRenderer* renderer);

    // Get last error message
    static const std::string& get_last_error();

private:
    static std::string s_last_error;
};

} // namespace engine::asset
