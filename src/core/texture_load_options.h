#pragma once
#include "core/texture_data.h"

// Load-time configuration for texture loading
struct TextureLoadOptions {
    // Semantic usage hint (affects auto-detection logic)
    TextureUsage usage = TextureUsage::Generic;

    // Texture type (2D, array, cubemap)
    TextureType type = TextureType::Texture2D;

    // Format control
    u32 desiredChannels = 0;        // 0 = keep original format
                                    // 1 = force R (grayscale)
                                    // 2 = force RG
                                    // 3 = force RGB
                                    // 4 = force RGBA

    VkFormat formatOverride = VK_FORMAT_UNDEFINED;  // Manual format override (auto-detect if undefined)

    // Texture flags (mipmap generation, sRGB override, etc.)
    TextureFlags flags = TextureFlags::GenerateMipmaps;

    // Anisotropic filtering level (0 = use global default, 1-16 = per-texture override)
    u32 anisotropyLevel = 0;

    // Compression hint for future GPU compression (Phase 2+)
    VkFormat compressionHint = VK_FORMAT_UNDEFINED;

    // Auto-detect sRGB from usage?
    // If true: Albedo/AO → sRGB, Normal/Roughness/Metalness/Height → linear
    // If false: Use explicit TextureFlags::SRGB flag
    bool autoDetectSRGB = true;

    // Flip image vertically on load (useful for OpenGL→Vulkan conversions)
    bool flipVertical = false;

    // Default constructor
    TextureLoadOptions() = default;

    // Convenience constructors for common use cases
    static TextureLoadOptions Albedo() {
        TextureLoadOptions opts;
        opts.usage = TextureUsage::Albedo;
        opts.autoDetectSRGB = true;
        return opts;
    }

    static TextureLoadOptions Normal() {
        TextureLoadOptions opts;
        opts.usage = TextureUsage::Normal;
        opts.autoDetectSRGB = true;
        return opts;
    }

    static TextureLoadOptions Roughness() {
        TextureLoadOptions opts;
        opts.usage = TextureUsage::Roughness;
        opts.desiredChannels = 1;  // Single channel
        opts.autoDetectSRGB = true;
        return opts;
    }

    static TextureLoadOptions Metalness() {
        TextureLoadOptions opts;
        opts.usage = TextureUsage::Metalness;
        opts.desiredChannels = 1;  // Single channel
        opts.autoDetectSRGB = true;
        return opts;
    }

    static TextureLoadOptions AO() {
        TextureLoadOptions opts;
        opts.usage = TextureUsage::AO;
        opts.desiredChannels = 1;  // Single channel
        opts.autoDetectSRGB = true;
        return opts;
    }

    static TextureLoadOptions Height() {
        TextureLoadOptions opts;
        opts.usage = TextureUsage::Height;
        opts.desiredChannels = 1;  // Single channel
        opts.autoDetectSRGB = true;
        return opts;
    }
};
