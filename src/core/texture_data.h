#pragma once
#include "core/types.h"
#include <vulkan/vulkan.h>

// Forward declaration
class VulkanTexture;

// Semantic usage hint for textures (affects sRGB auto-detection)
enum class TextureUsage : u8 {
    Albedo,        // Diffuse color map - typically sRGB
    Normal,        // Tangent-space normal map - always linear
    Roughness,     // Surface roughness - linear, R channel
    Metalness,     // Metallic property - linear, R channel
    AO,            // Ambient occlusion - linear or sRGB, R channel
    Height,        // Height/displacement map - linear, R channel
    Generic        // No assumptions, manual configuration
};

// Bitfield flags for texture configuration
enum class TextureFlags : u32 {
    None = 0,
    SRGB = 1 << 0,              // Override: force sRGB color space
    GenerateMipmaps = 1 << 1,   // Auto-generate mipmaps on GPU
    PrebakedMipmaps = 1 << 2,   // File contains mipmaps (DDS/KTX support - Phase 2)
    AnisotropyOverride = 1 << 3 // Use per-texture anisotropy level
};

// Bitwise operators for TextureFlags
inline TextureFlags operator|(TextureFlags a, TextureFlags b) {
    return static_cast<TextureFlags>(static_cast<u32>(a) | static_cast<u32>(b));
}

inline TextureFlags operator&(TextureFlags a, TextureFlags b) {
    return static_cast<TextureFlags>(static_cast<u32>(a) & static_cast<u32>(b));
}

inline TextureFlags operator^(TextureFlags a, TextureFlags b) {
    return static_cast<TextureFlags>(static_cast<u32>(a) ^ static_cast<u32>(b));
}

inline TextureFlags operator~(TextureFlags a) {
    return static_cast<TextureFlags>(~static_cast<u32>(a));
}

inline TextureFlags& operator|=(TextureFlags& a, TextureFlags b) {
    a = a | b;
    return a;
}

inline TextureFlags& operator&=(TextureFlags& a, TextureFlags b) {
    a = a & b;
    return a;
}

inline TextureFlags& operator^=(TextureFlags& a, TextureFlags b) {
    a = a ^ b;
    return a;
}

// Helper to check if a flag is set
inline bool HasFlag(TextureFlags flags, TextureFlags flag) {
    return (static_cast<u32>(flags) & static_cast<u32>(flag)) != 0;
}

// Texture dimensionality
enum class TextureType : u8 {
    Texture2D,
    TextureArray,
    Cubemap
};

// CPU-side texture data with format metadata
struct TextureData {
    // CPU-side pixel data
    u8* pixels = nullptr;
    u32 width = 0;
    u32 height = 0;
    u32 channels = 0;           // 1=R, 2=RG, 3=RGB, 4=RGBA
    u32 mipLevels = 1;

    // Format metadata
    TextureUsage usage = TextureUsage::Generic;
    TextureType type = TextureType::Texture2D;
    VkFormat formatOverride = VK_FORMAT_UNDEFINED;  // Auto-detect if VK_FORMAT_UNDEFINED
    TextureFlags flags = TextureFlags::None;
    u32 anisotropyLevel = 0;    // 0 = use global default

    // Compression hint for future GPU compression (Phase 2+)
    VkFormat compressionHint = VK_FORMAT_UNDEFINED;

    // GPU resources (populated by VulkanTexture)
    VulkanTexture* gpuTexture = nullptr;
    bool gpuUploaded = false;

    // Destructor to free pixel data
    ~TextureData();

    // Prevent copying (texture data can be large)
    TextureData(const TextureData&) = delete;
    TextureData& operator=(const TextureData&) = delete;

    // Allow moving
    TextureData(TextureData&& other) noexcept;
    TextureData& operator=(TextureData&& other) noexcept;

    // Default constructor
    TextureData() = default;
};
