#pragma once

#include <engine/core/math.hpp>
#include <engine/render/types.hpp>
#include <string>
#include <cstdint>

namespace engine::asset {

using namespace engine::core;
using namespace engine::render;

// Base asset class
struct Asset {
    std::string path;
    uint64_t last_modified = 0;

    virtual ~Asset() = default;
};

// Mesh asset
struct MeshAsset : Asset {
    MeshHandle handle;
    AABB bounds;
    uint32_t vertex_count = 0;
    uint32_t index_count = 0;
};

// Texture asset
struct TextureAsset : Asset {
    TextureHandle handle;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t channels = 0;
    uint32_t mip_levels = 1;
    bool has_alpha = false;
    bool is_hdr = false;
};

// Shader asset
struct ShaderAsset : Asset {
    ShaderHandle handle;
};

// Material asset (JSON-based)
struct MaterialAsset : Asset {
    MaterialHandle handle;
    ShaderHandle shader;
    std::vector<std::pair<std::string, TextureHandle>> textures;
};

// Audio asset
struct AudioAsset : Asset {
    std::vector<uint8_t> data;
    uint32_t sample_rate = 0;
    uint32_t channels = 0;
    uint32_t sample_count = 0;
    bool is_stream = false;
};

// Scene asset (JSON-based)
struct SceneAsset : Asset {
    std::string json_data;
};

// Prefab asset (JSON-based)
struct PrefabAsset : Asset {
    std::string json_data;
};

} // namespace engine::asset
