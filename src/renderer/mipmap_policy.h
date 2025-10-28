#pragma once

#include "core/types.h"
#include <vulkan/vulkan.h>

// Forward declarations
class VulkanContext;
enum class TextureUsage : u8;

// Mipmap generation method policy
enum class MipmapPolicy : u8 {
    Auto,           // Automatic selection based on usage, format, quality settings
    ForceBlit,      // Force GPU blit-based generation (fastest, simple linear filtering)
    ForceCompute,   // Force compute shader generation (PBR-aware, gamma-correct)
    ForceCPU        // Force CPU-based generation (slowest, guaranteed fallback)
};

// Quality vs. speed preference for mipmap generation
enum class MipmapQuality : u8 {
    High,       // Prefer compute shaders for best quality (gamma-correct, PBR-aware)
    Balanced,   // Balance quality and speed (use heuristics: size, format, usage)
    Fast        // Prefer blit for speed (use compute only when necessary)
};

// Actual method selected after policy evaluation
enum class MipmapMethod : u8 {
    Blit,       // GPU blit via vkCmdBlitImage
    Compute,    // Compute shader via VulkanMipmapCompute
    CPU         // CPU-based fallback
};

// Parameters for mipmap generation policy decision
struct MipmapGenerationParams {
    TextureUsage usage;         // Semantic usage (Albedo, Normal, Roughness, etc.)
    VkFormat format;            // Vulkan format
    MipmapPolicy policy;        // User-specified policy
    MipmapQuality quality;      // Quality preference
    u32 width;                  // Texture width
    u32 height;                 // Texture height
    VulkanContext* context;     // For capability queries
};

// Policy decision engine
// Returns the actual method to use based on policy, usage, format, and hardware capabilities
MipmapMethod SelectMipGenerator(const MipmapGenerationParams& params);
