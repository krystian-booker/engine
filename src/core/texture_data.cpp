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
    // Free CPU-side pixel data if allocated
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
}

TextureData& TextureData::operator=(TextureData&& other) noexcept {
    if (this != &other) {
        // Free existing resources
        if (pixels) {
            free(pixels);
        }

        // Move data
        pixels = other.pixels;
        width = other.width;
        height = other.height;
        channels = other.channels;
        mipLevels = other.mipLevels;
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
