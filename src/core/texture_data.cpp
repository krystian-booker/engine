#include "core/texture_data.h"
#include "renderer/vulkan_texture.h"
#include "renderer/mipmap_policy.h"
#include <cstdlib>
#include <cstring>

TextureData::TextureData()
    : pixels(nullptr)
    , width(0)
    , height(0)
    , channels(0)
    , mipLevels(1)
    , arrayLayers(1)
    , usage(TextureUsage::Generic)
    , type(TextureType::Texture2D)
    , formatOverride(VK_FORMAT_UNDEFINED)
    , flags(TextureFlags::None)
    , anisotropyLevel(0)
    , mipmapPolicy(MipmapPolicy::Auto)
    , qualityHint(MipmapQuality::Balanced)
    , compressionHint(VK_FORMAT_UNDEFINED)
    , gpuTexture(nullptr)
    , gpuUploaded(false)
{
}

TextureData::~TextureData() {
    // Free per-layer pixel buffers if allocated
    for (u8* layerData : layerPixels) {
        if (layerData) {
            free(layerData);
        }
    }
    layerPixels.clear();

    // Free packed pixel data if allocated
    if (pixels) {
        free(pixels);
        pixels = nullptr;
    }

    // GPU resources managed by VulkanTexture (destroyed by TextureManager)
}

TextureData::TextureData(TextureData&& other) noexcept
    : pixels(other.pixels)
    , width(other.width)
    , height(other.height)
    , channels(other.channels)
    , mipLevels(other.mipLevels)
    , arrayLayers(other.arrayLayers)
    , layerPixels(std::move(other.layerPixels))
    , usage(other.usage)
    , type(other.type)
    , formatOverride(other.formatOverride)
    , flags(other.flags)
    , anisotropyLevel(other.anisotropyLevel)
    , mipmapPolicy(other.mipmapPolicy)
    , qualityHint(other.qualityHint)
    , compressionHint(other.compressionHint)
    , gpuTexture(other.gpuTexture)
    , gpuUploaded(other.gpuUploaded)
{
    // Nullify moved-from object to prevent double-free
    other.pixels = nullptr;
    other.gpuTexture = nullptr;
    // layerPixels vector is already moved, no need to clear
}

TextureData& TextureData::operator=(TextureData&& other) noexcept {
    if (this != &other) {
        // Free existing resources
        for (u8* layerData : layerPixels) {
            if (layerData) {
                free(layerData);
            }
        }
        if (pixels) {
            free(pixels);
        }

        // Move data
        pixels = other.pixels;
        width = other.width;
        height = other.height;
        channels = other.channels;
        mipLevels = other.mipLevels;
        arrayLayers = other.arrayLayers;
        layerPixels = std::move(other.layerPixels);
        usage = other.usage;
        type = other.type;
        formatOverride = other.formatOverride;
        flags = other.flags;
        anisotropyLevel = other.anisotropyLevel;
        mipmapPolicy = other.mipmapPolicy;
        qualityHint = other.qualityHint;
        compressionHint = other.compressionHint;
        gpuTexture = other.gpuTexture;
        gpuUploaded = other.gpuUploaded;

        // Nullify moved-from object
        other.pixels = nullptr;
        other.gpuTexture = nullptr;
    }
    return *this;
}

bool TextureData::ValidateLayers() const {
    // If no layer pixels, validation passes (single texture case)
    if (layerPixels.empty()) {
        return true;
    }

    // Check that we have the expected number of layers
    if (layerPixels.size() != arrayLayers) {
        return false;
    }

    // All layers must be non-null
    for (const u8* layerData : layerPixels) {
        if (!layerData) {
            return false;
        }
    }

    // All validation checks passed
    return true;
}

bool TextureData::PackLayersIntoStagingBuffer() {
    // If no layers to pack, nothing to do
    if (layerPixels.empty()) {
        return true;
    }

    // Validate layers before packing
    if (!ValidateLayers()) {
        return false;
    }

    // Calculate total size needed for all layers
    const u64 layerSize = static_cast<u64>(width) * height * channels;
    const u64 totalSize = layerSize * arrayLayers;

    // Allocate contiguous buffer for all layers
    pixels = static_cast<u8*>(malloc(totalSize));
    if (!pixels) {
        return false;
    }

    // Copy each layer into the contiguous buffer
    for (u32 i = 0; i < arrayLayers; ++i) {
        u8* destOffset = pixels + (i * layerSize);
        memcpy(destOffset, layerPixels[i], layerSize);
    }

    // Free individual layer buffers and clear vector
    for (u8* layerData : layerPixels) {
        free(layerData);
    }
    layerPixels.clear();

    return true;
}

bool TextureData::ValidateCubemap() const {
    // Cubemap must have exactly 6 layers (faces: +X, -X, +Y, -Y, +Z, -Z)
    if (arrayLayers != 6) {
        return false;
    }

    // Cubemap faces must be square
    if (width != height) {
        return false;
    }

    // Must be marked as cubemap type
    if (type != TextureType::Cubemap) {
        return false;
    }

    // If layerPixels is populated, validate consistency
    if (!layerPixels.empty()) {
        if (!ValidateLayers()) {
            return false;
        }
    }

    return true;
}
