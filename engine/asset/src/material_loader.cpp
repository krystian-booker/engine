#include <engine/asset/material_loader.hpp>
#include <engine/asset/manager.hpp>
#include <engine/core/filesystem.hpp>
#include <engine/core/log.hpp>
#include <nlohmann/json.hpp>

// Note: CGLTF_IMPLEMENTATION is defined in gltf_importer.cpp
#include <cgltf.h>

// stb_image for decoding embedded textures (implementation is in manager.cpp)
#include <stb_image.h>

#include <cstring>

namespace engine::asset {

using namespace engine::core;
using namespace engine::render;
using json = nlohmann::json;

std::string MaterialLoader::s_last_error;

std::shared_ptr<MaterialAsset> MaterialLoader::load_from_json(
    const std::string& path,
    AssetManager& asset_manager,
    render::IRenderer* renderer)
{
    s_last_error.clear();

    if (!renderer) {
        s_last_error = "Renderer not initialized";
        return nullptr;
    }

    // Read JSON file
    auto content = FileSystem::read_text(path);
    if (content.empty()) {
        s_last_error = "Failed to read material file: " + path;
        return nullptr;
    }

    json mat_json;
    try {
        mat_json = json::parse(content);
    } catch (const json::exception& e) {
        s_last_error = "JSON parse error: " + std::string(e.what());
        return nullptr;
    }

    MaterialData mat_data;

    // Load shader if specified
    if (mat_json.contains("shader")) {
        std::string shader_path = mat_json["shader"].get<std::string>();
        auto shader = asset_manager.load_shader(shader_path);
        if (shader && shader->handle.valid()) {
            mat_data.shader = shader->handle;
        } else {
            log(LogLevel::Warn, ("Material references missing shader: " + shader_path).c_str());
        }
    }

    // Load textures
    std::vector<std::pair<std::string, TextureHandle>> texture_bindings;
    if (mat_json.contains("textures") && mat_json["textures"].is_object()) {
        for (auto& [slot_name, tex_path_val] : mat_json["textures"].items()) {
            if (!tex_path_val.is_string()) continue;

            std::string tex_path = tex_path_val.get<std::string>();
            auto texture = asset_manager.load_texture(tex_path);
            if (texture && texture->handle.valid()) {
                texture_bindings.emplace_back(slot_name, texture->handle);

                // Also add to material properties
                MaterialProperty prop;
                prop.type = MaterialPropertyType::Texture;
                prop.value.texture = texture->handle;
                mat_data.properties.emplace_back(slot_name, prop);
            } else {
                log(LogLevel::Warn, ("Material references missing texture: " + tex_path).c_str());
            }
        }
    }

    // Load scalar/vector properties
    if (mat_json.contains("properties") && mat_json["properties"].is_object()) {
        for (auto& [prop_name, prop_val] : mat_json["properties"].items()) {
            MaterialProperty prop;

            if (prop_val.is_number()) {
                prop.type = MaterialPropertyType::Float;
                prop.value.f = prop_val.get<float>();
            } else if (prop_val.is_array()) {
                size_t size = prop_val.size();
                if (size == 2) {
                    prop.type = MaterialPropertyType::Vec2;
                    prop.value.v2[0] = prop_val[0].get<float>();
                    prop.value.v2[1] = prop_val[1].get<float>();
                } else if (size == 3) {
                    prop.type = MaterialPropertyType::Vec3;
                    prop.value.v3[0] = prop_val[0].get<float>();
                    prop.value.v3[1] = prop_val[1].get<float>();
                    prop.value.v3[2] = prop_val[2].get<float>();
                } else if (size == 4) {
                    prop.type = MaterialPropertyType::Vec4;
                    prop.value.v4[0] = prop_val[0].get<float>();
                    prop.value.v4[1] = prop_val[1].get<float>();
                    prop.value.v4[2] = prop_val[2].get<float>();
                    prop.value.v4[3] = prop_val[3].get<float>();
                } else {
                    continue; // Unsupported array size
                }
            } else {
                continue; // Skip unsupported types
            }

            mat_data.properties.emplace_back(prop_name, prop);
        }
    }

    // Material flags
    if (mat_json.contains("double_sided") && mat_json["double_sided"].is_boolean()) {
        mat_data.double_sided = mat_json["double_sided"].get<bool>();
    }
    if (mat_json.contains("transparent") && mat_json["transparent"].is_boolean()) {
        mat_data.transparent = mat_json["transparent"].get<bool>();
    }

    // Create GPU material
    MaterialHandle handle = renderer->create_material(mat_data);
    if (!handle.valid()) {
        s_last_error = "Failed to create GPU material";
        return nullptr;
    }

    auto asset = std::make_shared<MaterialAsset>();
    asset->path = path;
    asset->handle = handle;
    asset->shader = mat_data.shader;
    asset->textures = std::move(texture_bindings);

    log(LogLevel::Debug, ("Loaded material: " + path).c_str());
    return asset;
}

std::shared_ptr<MaterialAsset> MaterialLoader::load_from_gltf(
    const std::string& gltf_path,
    uint32_t material_index,
    AssetManager& asset_manager,
    render::IRenderer* renderer)
{
    s_last_error.clear();

    if (!renderer) {
        s_last_error = "Renderer not initialized";
        return nullptr;
    }

    // Parse glTF file
    cgltf_options options{};
    cgltf_data* data = nullptr;

    cgltf_result result = cgltf_parse_file(&options, gltf_path.c_str(), &data);
    if (result != cgltf_result_success) {
        s_last_error = "Failed to parse glTF file: " + gltf_path;
        return nullptr;
    }

    // Load external buffers if needed
    result = cgltf_load_buffers(&options, data, gltf_path.c_str());
    if (result != cgltf_result_success) {
        cgltf_free(data);
        s_last_error = "Failed to load glTF buffers";
        return nullptr;
    }

    if (material_index >= data->materials_count) {
        cgltf_free(data);
        s_last_error = "Material index out of range";
        return nullptr;
    }

    const cgltf_material& gltf_mat = data->materials[material_index];

    // Extract base directory for texture paths
    std::string base_dir;
    size_t last_slash = gltf_path.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        base_dir = gltf_path.substr(0, last_slash + 1);
    }

    MaterialData mat_data;
    std::vector<std::pair<std::string, TextureHandle>> texture_bindings;

    // Helper to load texture (supports external files, embedded buffers, and data URIs)
    auto load_gltf_texture = [&](const cgltf_texture_view& tex_view, const std::string& slot_name) {
        if (!tex_view.texture || !tex_view.texture->image) return;

        const cgltf_image* image = tex_view.texture->image;
        TextureHandle tex_handle;

        // Case 1: Embedded in binary buffer (GLB or buffer-referenced)
        if (image->buffer_view) {
            const uint8_t* buffer_data = static_cast<const uint8_t*>(image->buffer_view->buffer->data);
            const uint8_t* image_data = buffer_data + image->buffer_view->offset;
            size_t image_size = image->buffer_view->size;

            int width, height, channels;
            unsigned char* pixels = stbi_load_from_memory(
                image_data,
                static_cast<int>(image_size),
                &width, &height, &channels, 4
            );

            if (pixels) {
                TextureData tex_data;
                tex_data.width = static_cast<uint32_t>(width);
                tex_data.height = static_cast<uint32_t>(height);
                tex_data.format = TextureFormat::RGBA8;
                tex_data.pixels.assign(pixels, pixels + width * height * 4);
                stbi_image_free(pixels);

                tex_handle = renderer->create_texture(tex_data);
                log(LogLevel::Debug, ("Loaded embedded texture: " + slot_name).c_str());
            }
        }
        // Case 2: Base64 data URI
        else if (image->uri && std::strncmp(image->uri, "data:", 5) == 0) {
            // Find the base64 data after "data:image/xxx;base64,"
            const char* base64_start = std::strstr(image->uri, ";base64,");
            if (base64_start) {
                base64_start += 8; // Skip ";base64,"

                // Decode base64
                size_t encoded_len = std::strlen(base64_start);
                std::vector<uint8_t> decoded;
                decoded.reserve((encoded_len * 3) / 4);

                static const unsigned char base64_table[256] = {
                    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
                    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
                    255,255,255,255,255,255,255,255,255,255,255, 62,255,255,255, 63,
                     52, 53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255,  0,255,255,
                    255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
                     15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255,255,
                    255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
                     41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255,
                    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
                    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
                    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
                    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
                    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
                    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
                    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
                    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
                };

                uint32_t accum = 0;
                int bits = 0;
                for (size_t i = 0; i < encoded_len; i++) {
                    unsigned char c = static_cast<unsigned char>(base64_start[i]);
                    if (c == '=' || base64_table[c] == 255) continue;
                    accum = (accum << 6) | base64_table[c];
                    bits += 6;
                    if (bits >= 8) {
                        bits -= 8;
                        decoded.push_back(static_cast<uint8_t>((accum >> bits) & 0xFF));
                    }
                }

                int width, height, channels;
                unsigned char* pixels = stbi_load_from_memory(
                    decoded.data(),
                    static_cast<int>(decoded.size()),
                    &width, &height, &channels, 4
                );

                if (pixels) {
                    TextureData tex_data;
                    tex_data.width = static_cast<uint32_t>(width);
                    tex_data.height = static_cast<uint32_t>(height);
                    tex_data.format = TextureFormat::RGBA8;
                    tex_data.pixels.assign(pixels, pixels + width * height * 4);
                    stbi_image_free(pixels);

                    tex_handle = renderer->create_texture(tex_data);
                    log(LogLevel::Debug, ("Loaded base64 texture: " + slot_name).c_str());
                }
            }
        }
        // Case 3: External file URI (existing behavior)
        else if (image->uri) {
            std::string tex_path = base_dir + image->uri;
            auto texture = asset_manager.load_texture(tex_path);
            if (texture && texture->handle.valid()) {
                tex_handle = texture->handle;
            }
        }

        // Add to material if texture was loaded successfully
        if (tex_handle.valid()) {
            texture_bindings.emplace_back(slot_name, tex_handle);

            MaterialProperty prop;
            prop.type = MaterialPropertyType::Texture;
            prop.value.texture = tex_handle;
            mat_data.properties.emplace_back(slot_name, prop);
        }
    };

    // PBR Metallic-Roughness workflow
    if (gltf_mat.has_pbr_metallic_roughness) {
        const auto& pbr = gltf_mat.pbr_metallic_roughness;

        // Base color texture
        load_gltf_texture(pbr.base_color_texture, "albedo");

        // Metallic-roughness texture
        load_gltf_texture(pbr.metallic_roughness_texture, "metallic_roughness");

        // Base color factor
        MaterialProperty base_color_prop;
        base_color_prop.type = MaterialPropertyType::Vec4;
        base_color_prop.value.v4[0] = pbr.base_color_factor[0];
        base_color_prop.value.v4[1] = pbr.base_color_factor[1];
        base_color_prop.value.v4[2] = pbr.base_color_factor[2];
        base_color_prop.value.v4[3] = pbr.base_color_factor[3];
        mat_data.properties.emplace_back("base_color", base_color_prop);

        // Metallic factor
        MaterialProperty metallic_prop;
        metallic_prop.type = MaterialPropertyType::Float;
        metallic_prop.value.f = pbr.metallic_factor;
        mat_data.properties.emplace_back("metallic", metallic_prop);

        // Roughness factor
        MaterialProperty roughness_prop;
        roughness_prop.type = MaterialPropertyType::Float;
        roughness_prop.value.f = pbr.roughness_factor;
        mat_data.properties.emplace_back("roughness", roughness_prop);
    }

    // Normal map
    load_gltf_texture(gltf_mat.normal_texture, "normal");

    // Occlusion map
    load_gltf_texture(gltf_mat.occlusion_texture, "occlusion");

    // Emissive
    load_gltf_texture(gltf_mat.emissive_texture, "emissive");
    if (gltf_mat.emissive_factor[0] > 0 || gltf_mat.emissive_factor[1] > 0 || gltf_mat.emissive_factor[2] > 0) {
        MaterialProperty emissive_prop;
        emissive_prop.type = MaterialPropertyType::Vec3;
        emissive_prop.value.v3[0] = gltf_mat.emissive_factor[0];
        emissive_prop.value.v3[1] = gltf_mat.emissive_factor[1];
        emissive_prop.value.v3[2] = gltf_mat.emissive_factor[2];
        mat_data.properties.emplace_back("emissive", emissive_prop);
    }

    // Material flags
    mat_data.double_sided = gltf_mat.double_sided;
    mat_data.transparent = (gltf_mat.alpha_mode == cgltf_alpha_mode_blend);

    cgltf_free(data);

    // Create GPU material
    MaterialHandle handle = renderer->create_material(mat_data);
    if (!handle.valid()) {
        s_last_error = "Failed to create GPU material from glTF";
        return nullptr;
    }

    auto asset = std::make_shared<MaterialAsset>();
    asset->path = gltf_path + "#material" + std::to_string(material_index);
    asset->handle = handle;
    asset->shader = mat_data.shader;
    asset->textures = std::move(texture_bindings);

    std::string mat_name = gltf_mat.name ? gltf_mat.name : std::to_string(material_index);
    log(LogLevel::Debug, ("Loaded glTF material: " + mat_name + " from " + gltf_path).c_str());

    return asset;
}

const std::string& MaterialLoader::get_last_error() {
    return s_last_error;
}

} // namespace engine::asset
