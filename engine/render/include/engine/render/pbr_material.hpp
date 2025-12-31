#pragma once

#include <engine/render/types.hpp>
#include <engine/core/math.hpp>

namespace engine::render {

using namespace engine::core;

// Blend modes for PBR materials
enum class BlendMode : uint8_t {
    Opaque,         // Fully opaque, no alpha blending
    AlphaTest,      // Binary transparency using alpha cutoff
    AlphaBlend,     // Standard alpha blending
    Additive,       // Additive blending (particles, effects)
    Multiply        // Multiplicative blending
};

// PBR Material data for metallic-roughness workflow
struct PBRMaterial {
    // Texture maps (invalid handle = use fallback values)
    TextureHandle albedo_map;           // Base color (RGB) + opacity (A)
    TextureHandle normal_map;           // Tangent-space normal
    TextureHandle metallic_roughness;   // Green = roughness, Blue = metallic (glTF convention)
    TextureHandle ao_map;               // Ambient occlusion
    TextureHandle emissive_map;         // Emission color

    // Fallback values (used when textures are not set)
    Vec4 albedo_color{1.0f, 1.0f, 1.0f, 1.0f};  // Base color + alpha
    float metallic = 0.0f;                       // 0 = dielectric, 1 = metal
    float roughness = 0.5f;                      // 0 = smooth, 1 = rough
    float ao = 1.0f;                             // Ambient occlusion multiplier
    Vec3 emissive{0.0f, 0.0f, 0.0f};            // Emissive color
    float emissive_intensity = 1.0f;             // Emissive intensity multiplier

    // Rendering flags
    BlendMode blend_mode = BlendMode::Opaque;
    float alpha_cutoff = 0.5f;                   // For AlphaTest blend mode
    bool double_sided = false;                   // Disable backface culling
    bool receive_shadows = true;
    bool cast_shadows = true;

    // UV transform (optional)
    Vec2 uv_offset{0.0f, 0.0f};
    Vec2 uv_scale{1.0f, 1.0f};
    float uv_rotation = 0.0f;
};

// GPU-packed light data for shader upload (16-byte aligned)
struct GPULightData {
    Vec4 position_type;      // xyz = position, w = type (0=dir, 1=point, 2=spot)
    Vec4 direction_range;    // xyz = direction, w = range
    Vec4 color_intensity;    // xyz = color, w = intensity
    Vec4 spot_params;        // x = inner angle, y = outer angle, z = shadow index, w = unused
};

// GPU-packed PBR material data for shader upload
struct GPUMaterialData {
    Vec4 albedo_color;       // xyz = base color, w = alpha
    Vec4 pbr_params;         // x = metallic, y = roughness, z = ao, w = alpha cutoff
    Vec4 emissive_color;     // xyz = emissive, w = intensity
};

// IBL (Image-Based Lighting) data
struct IBLData {
    TextureHandle irradiance_map;     // Diffuse IBL cubemap (low res)
    TextureHandle prefiltered_map;    // Specular IBL cubemap (high res, mipmapped)
    TextureHandle brdf_lut;           // BRDF integration LUT (2D texture)
    float intensity = 1.0f;
    float rotation = 0.0f;            // Environment rotation in radians
    uint32_t max_mip_level = 5;       // Number of mip levels in prefiltered map
};

// Convert engine LightData to GPU format
inline GPULightData packLightForGPU(const LightData& light) {
    GPULightData gpu;
    gpu.position_type = Vec4(light.position, static_cast<float>(light.type));
    gpu.direction_range = Vec4(light.direction, light.range);
    gpu.color_intensity = Vec4(light.color, light.intensity);
    gpu.spot_params = Vec4(
        light.spot_angle * 0.5f,    // Inner angle (half of spot angle)
        light.spot_angle,           // Outer angle
        light.cast_shadows ? 0.0f : -1.0f,  // Shadow index (-1 = no shadow)
        0.0f
    );
    return gpu;
}

// Convert PBRMaterial to GPU format
inline GPUMaterialData packMaterialForGPU(const PBRMaterial& mat) {
    GPUMaterialData gpu;
    gpu.albedo_color = mat.albedo_color;
    gpu.pbr_params = Vec4(mat.metallic, mat.roughness, mat.ao, mat.alpha_cutoff);
    gpu.emissive_color = Vec4(mat.emissive, mat.emissive_intensity);
    return gpu;
}

} // namespace engine::render
