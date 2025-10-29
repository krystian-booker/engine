#pragma once

#include "core/types.h"
#include "core/math.h"

// GPU-side material structure for SSBO (std430 layout)
// IMPORTANT: Must match shader layout exactly
// std430 alignment rules:
//   - scalars: base alignment
//   - vec2: 8 bytes
//   - vec3/vec4: 16 bytes
//   - struct: largest member alignment, rounded up to vec4
struct GPUMaterial {
    // Texture descriptor indices (for bindless access)
    u32 albedoIndex;        // Index into bindless texture array
    u32 normalIndex;
    u32 metalRoughIndex;
    u32 aoIndex;

    u32 emissiveIndex;
    u32 flags;              // MaterialFlags as u32
    u32 padding1;           // Align to 16 bytes
    u32 padding2;

    // PBR parameters (16-byte aligned)
    Vec4 albedoTint;        // RGBA tint color
    Vec4 emissiveFactor;    // RGB emissive + intensity in w

    // Scalar parameters (pack into vec4 for alignment)
    f32 metallicFactor;
    f32 roughnessFactor;
    f32 normalScale;
    f32 aoStrength;

    // Total size: 80 bytes (5 * vec4)
    // Fits within typical alignment requirements
};

// Verify struct size at compile time
static_assert(sizeof(GPUMaterial) == 80, "GPUMaterial size must be 80 bytes for std430 layout");
static_assert(sizeof(GPUMaterial) % 16 == 0, "GPUMaterial must be 16-byte aligned");
