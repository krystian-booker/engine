#pragma once

#include "core/types.h"
#include "core/resource_handle.h"
#include <string>

// Environment probe for Image-Based Lighting
// Stores references to IBL cubemaps (irradiance, prefiltered, BRDF LUT)
struct EnvironmentProbe {
    // HDR environment map (source)
    TextureHandle environmentMap = TextureHandle::Invalid;

    // Precomputed IBL maps
    TextureHandle irradianceMap = TextureHandle::Invalid;     // Diffuse IBL (32x32 cubemap)
    TextureHandle prefilteredMap = TextureHandle::Invalid;    // Specular IBL (128x128 cubemap with mipmaps)
    TextureHandle brdfLUT = TextureHandle::Invalid;           // BRDF integration LUT (512x512 2D)

    // Probe properties
    f32 intensity = 1.0f;        // Global intensity multiplier
    f32 radius = 100.0f;         // Influence radius (for local probes)
    bool isGlobal = true;        // Global probe (affects entire scene) vs local probe

    // Source path (for hot-reload)
    std::string sourcePath;

    // Runtime flags
    bool needsUpdate = false;    // Regenerate IBL maps if true
};
