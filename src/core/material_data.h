#pragma once

#include "types.h"
#include "math.h"
#include "resource_handle.h"

// Material flags for rendering behavior
enum class MaterialFlags : u32 {
    None           = 0,
    DoubleSided    = 1 << 0,  // Disable backface culling
    AlphaBlend     = 1 << 1,  // Enable alpha blending
    AlphaMask      = 1 << 2,  // Use alpha mask (discard)
    AlphaTest      = 1 << 3,  // Use alpha testing
};

// Bitwise operators for MaterialFlags
inline MaterialFlags operator|(MaterialFlags a, MaterialFlags b) {
    return static_cast<MaterialFlags>(static_cast<u32>(a) | static_cast<u32>(b));
}

inline MaterialFlags operator&(MaterialFlags a, MaterialFlags b) {
    return static_cast<MaterialFlags>(static_cast<u32>(a) & static_cast<u32>(b));
}

inline MaterialFlags operator~(MaterialFlags a) {
    return static_cast<MaterialFlags>(~static_cast<u32>(a));
}

inline MaterialFlags& operator|=(MaterialFlags& a, MaterialFlags b) {
    a = a | b;
    return a;
}

inline MaterialFlags& operator&=(MaterialFlags& a, MaterialFlags b) {
    a = a & b;
    return a;
}

// Helper functions for flag manipulation
inline bool HasFlag(MaterialFlags flags, MaterialFlags flag) {
    return (flags & flag) != MaterialFlags::None;
}

inline void SetFlag(MaterialFlags& flags, MaterialFlags flag) {
    flags |= flag;
}

inline void ClearFlag(MaterialFlags& flags, MaterialFlags flag) {
    flags &= ~flag;
}

// CPU-side material data
struct MaterialData {
    // Texture handles (CPU-side)
    TextureHandle albedo = TextureHandle::Invalid;
    TextureHandle normal = TextureHandle::Invalid;
    TextureHandle metalRough = TextureHandle::Invalid;  // R=roughness, G=metalness, B=AO (optional)
    TextureHandle ao = TextureHandle::Invalid;
    TextureHandle emissive = TextureHandle::Invalid;

    // PBR parameters
    Vec4 albedoTint = Vec4(1.0f, 1.0f, 1.0f, 1.0f);      // Base color multiplier
    Vec4 emissiveFactor = Vec4(0.0f, 0.0f, 0.0f, 0.0f);  // Emissive color and intensity
    f32 metallicFactor = 0.0f;                            // Metallic value (0=dielectric, 1=conductor)
    f32 roughnessFactor = 0.5f;                           // Roughness value (0=smooth, 1=rough)
    f32 normalScale = 1.0f;                               // Normal map intensity
    f32 aoStrength = 1.0f;                                // Ambient occlusion strength

    // Rendering flags
    MaterialFlags flags = MaterialFlags::None;

    // Material index in GPU SSBO (assigned by MaterialManager)
    u32 gpuMaterialIndex = 0xFFFFFFFF;

    // Default constructor with sensible defaults
    MaterialData() = default;

    // Check if material uses alpha
    bool UsesAlpha() const {
        return HasFlag(flags, MaterialFlags::AlphaBlend) ||
               HasFlag(flags, MaterialFlags::AlphaMask) ||
               HasFlag(flags, MaterialFlags::AlphaTest);
    }

    // Check if material is double-sided
    bool IsDoubleSided() const {
        return HasFlag(flags, MaterialFlags::DoubleSided);
    }
};
