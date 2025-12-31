#pragma once

#include <engine/core/math.hpp>
#include <cstdint>
#include <vector>
#include <string>
#include <utility>

namespace engine::render {

using namespace engine::core;

// Opaque handles for GPU resources
struct MeshHandle {
    uint32_t id = UINT32_MAX;
    bool valid() const { return id != UINT32_MAX; }
};

struct TextureHandle {
    uint32_t id = UINT32_MAX;
    bool valid() const { return id != UINT32_MAX; }
};

struct ShaderHandle {
    uint32_t id = UINT32_MAX;
    bool valid() const { return id != UINT32_MAX; }
};

struct MaterialHandle {
    uint32_t id = UINT32_MAX;
    bool valid() const { return id != UINT32_MAX; }
};

// Vertex format (static meshes)
struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec2 texcoord;
    Vec4 color{1.0f};
    Vec3 tangent{0.0f};
};

// Skinned vertex format (animated meshes)
struct SkinnedVertex {
    Vec3 position;
    Vec3 normal;
    Vec2 texcoord;
    Vec4 color{1.0f};
    Vec3 tangent{0.0f};
    IVec4 bone_indices{0};   // Up to 4 bone influences (indices into bone array)
    Vec4 bone_weights{0.0f}; // Corresponding weights (should sum to 1.0)
};

// Mesh data for creating meshes
struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    AABB bounds;
};

// Skinned mesh data for creating animated meshes
struct SkinnedMeshData {
    std::vector<SkinnedVertex> vertices;
    std::vector<uint32_t> indices;
    AABB bounds;
    uint32_t bone_count = 0;
};

// Texture formats
enum class TextureFormat : uint8_t {
    RGBA8,
    RGBA16F,
    RGBA32F,
    R8,
    RG8,
    Depth24,
    Depth32F,
    BC1,  // DXT1
    BC3,  // DXT5
    BC7
};

// Texture data for creating textures
struct TextureData {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;
    uint32_t mip_levels = 1;
    TextureFormat format = TextureFormat::RGBA8;
    std::vector<uint8_t> pixels;
    bool is_cubemap = false;
};

// Shader types
enum class ShaderType : uint8_t {
    Vertex,
    Fragment,
    Compute
};

// Shader data for creating shaders
struct ShaderData {
    std::vector<uint8_t> vertex_binary;
    std::vector<uint8_t> fragment_binary;
};

// Material property types
enum class MaterialPropertyType : uint8_t {
    Float,
    Vec2,
    Vec3,
    Vec4,
    Mat4,
    Texture
};

// Material property
struct MaterialProperty {
    MaterialPropertyType type;
    union {
        float f;
        float v2[2];
        float v3[3];
        float v4[4];
        float m4[16];
        TextureHandle texture;
    } value;
};

// Material data for creating materials
struct MaterialData {
    ShaderHandle shader;
    std::vector<std::pair<std::string, MaterialProperty>> properties;
    bool double_sided = false;
    bool transparent = false;
};

// Draw call structure
struct DrawCall {
    MeshHandle mesh;
    MaterialHandle material;
    Mat4 transform;
    uint8_t render_layer = 0;
    bool cast_shadows = true;
};

// Light data for rendering
struct LightData {
    Vec3 position;
    Vec3 direction;
    Vec3 color;
    float intensity;
    float range;
    float spot_angle;
    uint8_t type;  // 0=directional, 1=point, 2=spot
    bool cast_shadows;
};

// Primitive mesh types for quick creation
enum class PrimitiveMesh : uint8_t {
    Cube,
    Sphere,
    Plane,
    Cylinder,
    Cone,
    Quad
};

} // namespace engine::render
