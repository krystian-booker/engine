#pragma once

#include "core/types.h"

// Shadow filtering modes
enum class ShadowFilterMode : u32 {
    PCF = 0,                // Percentage-Closer Filtering (hardware + software)
    PCSS = 1,               // Percentage-Closer Soft Shadows (variable penumbra)
    ContactHardening = 2,   // Distance-based PCF radius (simpler than PCSS)
    EVSM = 3,               // Exponential Variance Shadow Maps
    MomentShadow = 4        // Moment Shadow Maps (4-moment)
};

// Shadow map quality levels
enum class ShadowQuality : u32 {
    Low = 0,      // 512x512
    Medium = 1,   // 1024x1024
    High = 2,     // 2048x2048
    Ultra = 3     // 4096x4096
};

// Shadow update frequency
enum class ShadowUpdateMode : u32 {
    Static = 0,   // Never update after initial render (baked)
    Dynamic = 1,  // Update every frame
    OnDemand = 2  // Update only when flagged dirty
};

// LOD level for shadow maps
enum class ShadowLOD : u32 {
    LOD0 = 0,  // 2048x2048 (or configured max resolution)
    LOD1 = 1,  // 1024x1024
    LOD2 = 2,  // 512x512
    LOD3 = 3,  // 256x256
    COUNT = 4
};

// Per-light shadow configuration
struct LightShadowConfig {
    bool castsShadows = true;                           // Whether this light casts shadows
    ShadowFilterMode filterMode = ShadowFilterMode::PCF; // Filtering technique
    ShadowQuality quality = ShadowQuality::High;         // Base quality level
    ShadowUpdateMode updateMode = ShadowUpdateMode::Dynamic; // Update frequency
    ShadowLOD currentLOD = ShadowLOD::LOD0;             // Current LOD level (runtime)

    // Filter-specific parameters
    f32 pcfRadius = 2.0f;          // PCF kernel radius (in pixels)
    f32 pcssSearchRadius = 5.0f;   // PCSS blocker search radius
    f32 pcssPenumbraScale = 1.0f;  // PCSS penumbra multiplier
    f32 evsmExponent = 40.0f;      // EVSM exponential warp factor
    f32 evsmLightBleedReduction = 0.3f; // EVSM light bleeding reduction

    // Bias parameters
    f32 depthBias = 0.005f;        // Depth bias to prevent shadow acne
    f32 normalBias = 0.01f;        // Normal-based bias offset

    // LOD parameters (runtime managed)
    f32 distanceToCamera = 0.0f;   // Distance from camera (for LOD selection)
    bool isDirty = true;            // Whether shadow map needs update
    u32 lastUpdateFrame = 0;        // Frame number of last update
};

// Global shadow system configuration
struct ShadowSystemConfig {
    // Shadow atlas settings
    bool useAtlas = true;                   // Use shadow atlas instead of separate maps
    u32 atlasSize = 4096;                   // Atlas texture resolution
    u32 atlasLayers = 4;                    // Number of atlas array layers

    // LOD system settings
    bool enableLOD = true;                  // Enable dynamic LOD system
    f32 lodDistances[4] = {10.0f, 25.0f, 50.0f, 100.0f}; // LOD transition distances
    f32 lodBias = 0.0f;                     // Bias for LOD selection (-1.0 to 1.0)

    // Static shadow caching
    bool enableStaticCaching = true;        // Enable static shadow caching
    u32 maxStaticShadows = 16;              // Max number of cached static shadows
    u32 staticCacheRefreshFrames = 600;     // Frames before refreshing static cache

    // Screen-space contact shadows
    bool enableContactShadows = true;       // Enable screen-space contact shadows
    f32 contactShadowLength = 0.5f;         // Max ray length in world units
    u32 contactShadowSamples = 8;           // Ray march sample count
    f32 contactShadowIntensity = 1.0f;      // Contact shadow opacity multiplier

    // Cascaded shadow maps (directional lights)
    u32 numCascades = 4;                    // Number of CSM cascades
    f32 cascadeSplitLambda = 0.75f;         // Linear (0) to logarithmic (1) split
    bool stabilizeCascades = true;          // Prevent cascade swimming

    // Performance limits
    u32 maxPointLightShadows = 4;           // Max point lights with shadows
    u32 maxSpotLightShadows = 8;            // Max spot lights with shadows
    u32 maxShadowUpdatesPerFrame = 4;       // Max shadow map updates per frame

    // Debug settings
    bool visualizeC

ascades = false;        // Show cascade debug colors
    bool visualizeLODs = false;             // Show LOD debug colors
    bool showShadowStats = false;           // Display shadow statistics
};

// Helper functions for resolution calculation
inline u32 GetResolutionForQuality(ShadowQuality quality) {
    switch (quality) {
        case ShadowQuality::Low:    return 512;
        case ShadowQuality::Medium: return 1024;
        case ShadowQuality::High:   return 2048;
        case ShadowQuality::Ultra:  return 4096;
        default:                    return 1024;
    }
}

inline u32 GetResolutionForLOD(ShadowLOD lod, u32 baseResolution) {
    u32 lodLevel = static_cast<u32>(lod);
    return baseResolution >> lodLevel;  // Divide by 2^lodLevel
}

inline ShadowLOD SelectLODForDistance(f32 distance, const ShadowSystemConfig& config) {
    if (!config.enableLOD) {
        return ShadowLOD::LOD0;
    }

    // Apply LOD bias
    f32 biasedDistance = distance * (1.0f - config.lodBias * 0.5f);

    if (biasedDistance < config.lodDistances[0]) return ShadowLOD::LOD0;
    if (biasedDistance < config.lodDistances[1]) return ShadowLOD::LOD1;
    if (biasedDistance < config.lodDistances[2]) return ShadowLOD::LOD2;
    return ShadowLOD::LOD3;
}

// Check if shadow map needs update this frame
inline bool ShouldUpdateShadow(const LightShadowConfig& config, u32 currentFrame) {
    switch (config.updateMode) {
        case ShadowUpdateMode::Static:
            return config.lastUpdateFrame == 0;  // Only first frame
        case ShadowUpdateMode::Dynamic:
            return true;  // Every frame
        case ShadowUpdateMode::OnDemand:
            return config.isDirty;  // Only when dirty
        default:
            return true;
    }
}
