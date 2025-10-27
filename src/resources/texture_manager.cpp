#include "resources/texture_manager.h"
#include "resources/image_loader.h"
#include <iostream>
#include <cstring>

namespace TextureConfig {
    u32 g_DefaultAnisotropy = 16;

    void SetDefaultAnisotropy(u32 level) {
        if (level < 1) level = 1;
        if (level > 16) level = 16;
        g_DefaultAnisotropy = level;
    }

    u32 GetDefaultAnisotropy() {
        return g_DefaultAnisotropy;
    }
}

TextureManager::TextureManager() {
    // Initialize default load options
    m_DefaultOptions.usage = TextureUsage::Generic;
    m_DefaultOptions.flags = TextureFlags::GenerateMipmaps;
    m_DefaultOptions.autoDetectSRGB = true;

    // Default textures will be created on-demand
    m_WhiteTexture = TextureHandle::Invalid;
    m_BlackTexture = TextureHandle::Invalid;
    m_NormalMapTexture = TextureHandle::Invalid;
}

TextureHandle TextureManager::Load(const std::string& filepath, const TextureLoadOptions& options) {
    // Check cache first
    TextureHandle existing = GetHandle(filepath);
    if (IsValid(existing)) {
        return existing;
    }

    // Load image data using ImageLoader
    ImageData imageData = ImageLoader::LoadImage(filepath, options);
    if (!imageData.IsValid()) {
        std::cerr << "TextureManager::Load failed to load image: " << filepath << std::endl;
        return TextureHandle::Invalid;
    }

    // Create TextureData
    auto textureData = std::make_unique<TextureData>();
    textureData->pixels = imageData.pixels;
    textureData->width = imageData.width;
    textureData->height = imageData.height;
    textureData->channels = imageData.channels;
    textureData->usage = options.usage;
    textureData->type = options.type;
    textureData->formatOverride = options.formatOverride;
    textureData->flags = options.flags;
    textureData->compressionHint = options.compressionHint;

    // Set anisotropy level (0 means use global default)
    if (HasFlag(options.flags, TextureFlags::AnisotropyOverride)) {
        textureData->anisotropyLevel = options.anisotropyLevel;
    } else {
        textureData->anisotropyLevel = 0;  // Will use global default
    }

    // Calculate mip levels if GenerateMipmaps flag is set
    if (HasFlag(options.flags, TextureFlags::GenerateMipmaps)) {
        u32 maxDim = std::max(imageData.width, imageData.height);
        textureData->mipLevels = static_cast<u32>(std::floor(std::log2(maxDim))) + 1;
    } else {
        textureData->mipLevels = 1;
    }

    // Note: GPU upload will happen when VulkanTexture::Create is called by the renderer

    // Add to resource manager
    TextureHandle handle = Create(std::move(textureData));

    return handle;
}

std::unique_ptr<TextureData> TextureManager::LoadResource(const std::string& filepath) {
    // Use default options for generic loads
    ImageData imageData = ImageLoader::LoadImage(filepath, m_DefaultOptions);
    if (!imageData.IsValid()) {
        return nullptr;
    }

    auto textureData = std::make_unique<TextureData>();
    textureData->pixels = imageData.pixels;
    textureData->width = imageData.width;
    textureData->height = imageData.height;
    textureData->channels = imageData.channels;
    textureData->usage = m_DefaultOptions.usage;
    textureData->type = m_DefaultOptions.type;
    textureData->flags = m_DefaultOptions.flags;

    if (HasFlag(m_DefaultOptions.flags, TextureFlags::GenerateMipmaps)) {
        u32 maxDim = std::max(imageData.width, imageData.height);
        textureData->mipLevels = static_cast<u32>(std::floor(std::log2(maxDim))) + 1;
    } else {
        textureData->mipLevels = 1;
    }

    return textureData;
}

TextureHandle TextureManager::CreateSolid(u32 width, u32 height, const Vec4& color, TextureUsage usage) {
    auto textureData = std::make_unique<TextureData>();
    textureData->width = width;
    textureData->height = height;
    textureData->channels = 4;  // RGBA
    textureData->usage = usage;
    textureData->type = TextureType::Texture2D;
    textureData->flags = TextureFlags::None;  // No mipmaps for solid colors
    textureData->mipLevels = 1;

    // Allocate pixel data
    const u32 pixelCount = width * height;
    const u32 dataSize = pixelCount * 4;
    textureData->pixels = static_cast<u8*>(malloc(dataSize));

    // Convert color from [0,1] to [0,255]
    u8 r = static_cast<u8>(color.r * 255.0f);
    u8 g = static_cast<u8>(color.g * 255.0f);
    u8 b = static_cast<u8>(color.b * 255.0f);
    u8 a = static_cast<u8>(color.a * 255.0f);

    // Fill with color
    for (u32 i = 0; i < pixelCount; ++i) {
        textureData->pixels[i * 4 + 0] = r;
        textureData->pixels[i * 4 + 1] = g;
        textureData->pixels[i * 4 + 2] = b;
        textureData->pixels[i * 4 + 3] = a;
    }

    return Create(std::move(textureData));
}

TextureHandle TextureManager::CreateSinglePixel(u8 r, u8 g, u8 b, u8 a, TextureUsage usage) {
    auto textureData = std::make_unique<TextureData>();
    textureData->width = 1;
    textureData->height = 1;
    textureData->channels = 4;
    textureData->usage = usage;
    textureData->type = TextureType::Texture2D;
    textureData->flags = TextureFlags::None;
    textureData->mipLevels = 1;

    textureData->pixels = static_cast<u8*>(malloc(4));
    textureData->pixels[0] = r;
    textureData->pixels[1] = g;
    textureData->pixels[2] = b;
    textureData->pixels[3] = a;

    return Create(std::move(textureData));
}

TextureHandle TextureManager::CreateWhite() {
    if (!IsValid(m_WhiteTexture)) {
        m_WhiteTexture = CreateSinglePixel(255, 255, 255, 255, TextureUsage::Generic);
    }
    return m_WhiteTexture;
}

TextureHandle TextureManager::CreateBlack() {
    if (!IsValid(m_BlackTexture)) {
        m_BlackTexture = CreateSinglePixel(0, 0, 0, 255, TextureUsage::Generic);
    }
    return m_BlackTexture;
}

TextureHandle TextureManager::CreateNormalMap() {
    if (!IsValid(m_NormalMapTexture)) {
        // Neutral normal map: (0.5, 0.5, 1.0) in [0,1] → (127, 127, 255) in [0,255]
        // This represents a normal pointing straight up in tangent space: (0, 0, 1)
        m_NormalMapTexture = CreateSinglePixel(127, 127, 255, 255, TextureUsage::Normal);
    }
    return m_NormalMapTexture;
}
