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
    TextureFormat format = TextureFormat::RGBA8;  // Actual texture format for memory calculations
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

// Animation channel target path
enum class AnimationPath {
    Translation,
    Rotation,
    Scale
};

// Animation interpolation mode
enum class AnimationInterpolation {
    Step,
    Linear,
    CubicSpline
};

// Animation channel (animates a single property of a joint)
struct AnimationChannel {
    int32_t target_joint = -1;
    AnimationPath path = AnimationPath::Translation;
    AnimationInterpolation interpolation = AnimationInterpolation::Linear;
    std::vector<float> times;
    std::vector<float> values;  // Interleaved values (vec3 for trans/scale, vec4 for rotation)
};

// Animation asset
struct AnimationAsset : Asset {
    std::string name;
    float duration = 0.0f;
    std::vector<AnimationChannel> channels;
};

// Skeleton joint
struct SkeletonJoint {
    std::string name;
    int32_t parent_index = -1;  // -1 for root
    Mat4 inverse_bind_matrix;
    Mat4 local_transform;
};

// Skeleton asset (for skeletal animation)
struct SkeletonAsset : Asset {
    std::string name;
    std::vector<SkeletonJoint> joints;
};

} // namespace engine::asset
