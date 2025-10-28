#pragma once
#include "core/types.h"

// Sampler filter modes
enum class SamplerFilter : u8 {
    Nearest = 0,  // Point sampling
    Linear = 1    // Bilinear filtering
};

// Sampler address/wrap modes
enum class SamplerAddressMode : u8 {
    Repeat = 0,         // Repeat texture coordinates
    MirroredRepeat = 1, // Mirror and repeat
    ClampToEdge = 2,    // Clamp to edge pixels
    ClampToBorder = 3,  // Clamp to border color
    MirrorClampToEdge = 4
};

// Border color for ClampToBorder mode
enum class SamplerBorderColor : u8 {
    TransparentBlack = 0,  // (0, 0, 0, 0)
    OpaqueBlack = 1,       // (0, 0, 0, 1)
    OpaqueWhite = 2        // (1, 1, 1, 1)
};

// Mipmap filter modes
enum class SamplerMipmapMode : u8 {
    Nearest = 0,  // Select nearest mip level
    Linear = 1    // Linear interpolation between mip levels (trilinear)
};

// Complete sampler configuration
struct SamplerSettings {
    // Magnification/minification filters
    SamplerFilter magFilter = SamplerFilter::Linear;
    SamplerFilter minFilter = SamplerFilter::Linear;

    // Address modes for U, V, W coordinates
    SamplerAddressMode addressModeU = SamplerAddressMode::Repeat;
    SamplerAddressMode addressModeV = SamplerAddressMode::Repeat;
    SamplerAddressMode addressModeW = SamplerAddressMode::Repeat;

    // Anisotropic filtering
    bool anisotropyEnable = true;
    f32 maxAnisotropy = 16.0f;  // Will be clamped to device max

    // Border color (for ClampToBorder mode)
    SamplerBorderColor borderColor = SamplerBorderColor::OpaqueBlack;

    // Mipmap settings
    SamplerMipmapMode mipmapMode = SamplerMipmapMode::Linear;
    f32 mipLodBias = 0.0f;
    f32 minLod = 0.0f;
    f32 maxLod = 1000.0f;  // Large value = use all available mips

    // Comparison mode (for shadow sampling)
    bool compareEnable = false;

    // Unnormalized coordinates (false = use [0,1], true = use [0,width/height])
    bool unnormalizedCoordinates = false;

    // ========================================================================
    // Convenience constructors
    // ========================================================================

    // Default: Linear filtering with anisotropy and repeat wrap
    static SamplerSettings Default() {
        return SamplerSettings{};
    }

    // Nearest filtering for pixel-perfect sampling
    static SamplerSettings Nearest() {
        SamplerSettings settings;
        settings.magFilter = SamplerFilter::Nearest;
        settings.minFilter = SamplerFilter::Nearest;
        settings.mipmapMode = SamplerMipmapMode::Nearest;
        settings.anisotropyEnable = false;
        return settings;
    }

    // Clamped edges (for UI, fullscreen quads)
    static SamplerSettings Clamped() {
        SamplerSettings settings;
        settings.addressModeU = SamplerAddressMode::ClampToEdge;
        settings.addressModeV = SamplerAddressMode::ClampToEdge;
        settings.addressModeW = SamplerAddressMode::ClampToEdge;
        return settings;
    }

    // Mirror repeat for seamless tiling
    static SamplerSettings Mirrored() {
        SamplerSettings settings;
        settings.addressModeU = SamplerAddressMode::MirroredRepeat;
        settings.addressModeV = SamplerAddressMode::MirroredRepeat;
        settings.addressModeW = SamplerAddressMode::MirroredRepeat;
        return settings;
    }

    // High quality: Linear + anisotropy 16x
    static SamplerSettings HighQuality() {
        SamplerSettings settings;
        settings.maxAnisotropy = 16.0f;
        return settings;
    }

    // Low quality: Linear without anisotropy
    static SamplerSettings LowQuality() {
        SamplerSettings settings;
        settings.anisotropyEnable = false;
        return settings;
    }

    // For shadow maps with comparison
    static SamplerSettings Shadow() {
        SamplerSettings settings;
        settings.addressModeU = SamplerAddressMode::ClampToBorder;
        settings.addressModeV = SamplerAddressMode::ClampToBorder;
        settings.addressModeW = SamplerAddressMode::ClampToBorder;
        settings.borderColor = SamplerBorderColor::OpaqueWhite;
        settings.compareEnable = true;
        settings.anisotropyEnable = false;
        return settings;
    }
};
