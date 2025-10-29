#include "resources/texture_manager.h"
#include "resources/image_loader.h"
#include "resources/material_manager.h"
#include "renderer/mipmap_policy.h"
#include "renderer/vulkan_context.h"
#include "renderer/vulkan_transfer_queue.h"
#include "renderer/vulkan_staging_pool.h"
#include "renderer/vulkan_texture.h"
#include "core/job_system.h"
#include "platform/platform.h"
#include <iostream>
#include <cstring>
#include <algorithm>

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

    MipmapQuality g_DefaultMipmapQuality = MipmapQuality::Balanced;

    void SetDefaultMipmapQuality(MipmapQuality quality) {
        g_DefaultMipmapQuality = quality;
    }

    MipmapQuality GetDefaultMipmapQuality() {
        return g_DefaultMipmapQuality;
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
    m_MetalRoughTexture = TextureHandle::Invalid;
    m_PlaceholderTexture = TextureHandle::Invalid;
    m_ErrorTexture = TextureHandle::Invalid;

    // Initialize async loading state
    m_AsyncMutex = Platform::CreateMutex();
    m_GPUUploadMutex = Platform::CreateMutex();

    // Create placeholder and error textures (8x8 checkerboards)
    // Placeholder: Gray and white (neutral, indicates loading)
    m_PlaceholderTexture = CreateCheckerboard(8, Vec4(0.7f, 0.7f, 0.7f, 1.0f), Vec4(0.9f, 0.9f, 0.9f, 1.0f));

    // Error: Magenta and black (highly visible, indicates error)
    m_ErrorTexture = CreateCheckerboard(8, Vec4(1.0f, 0.0f, 1.0f, 1.0f), Vec4(0.0f, 0.0f, 0.0f, 1.0f));
}

void TextureManager::InitAsyncPipeline(VulkanContext* context, VulkanTransferQueue* transferQueue, VulkanStagingPool* stagingPool) {
    m_VulkanContext = context;
    m_TransferQueue = transferQueue;
    m_StagingPool = stagingPool;
    std::cout << "TextureManager async GPU upload pipeline initialized" << std::endl;
}

void TextureManager::ShutdownAsyncPipeline() {
    // Wait for all pending GPU uploads to complete
    if (m_TransferQueue && m_VulkanContext) {
        // Wait for all timeline values to complete
        if (m_VulkanContext) {
            VkDevice device = m_VulkanContext->GetDevice();
            vkDeviceWaitIdle(device);
        }
    }

    m_GPUUploadQueue.clear();
    m_VulkanContext = nullptr;
    m_TransferQueue = nullptr;
    m_StagingPool = nullptr;

    std::cout << "TextureManager async GPU upload pipeline shut down" << std::endl;
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
    textureData->samplerSettings = options.samplerSettings;

    // Store source path for hot reload
    textureData->sourcePaths.push_back(filepath);

    // Set anisotropy level (0 means use global default, DEPRECATED)
    if (HasFlag(options.flags, TextureFlags::AnisotropyOverride)) {
        textureData->anisotropyLevel = options.anisotropyLevel;
    } else {
        textureData->anisotropyLevel = 0;  // Will use global default
    }

    // Set mipmap generation policy and quality
    if (options.overrideMipmapPolicy) {
        textureData->mipmapPolicy = options.mipmapPolicy;
    }
    // Note: mipmapPolicy already initialized to Auto in TextureData constructor

    if (options.overrideQualityHint) {
        textureData->qualityHint = options.qualityHint;
    } else {
        textureData->qualityHint = TextureConfig::GetDefaultMipmapQuality();
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

TextureHandle TextureManager::LoadFromMemory(
    const u8* buffer,
    size_t bufferSize,
    const std::string& debugName,
    const TextureLoadOptions& options)
{
    // Check cache first (using debugName as key)
    TextureHandle existing = GetHandle(debugName);
    if (IsValid(existing)) {
        return existing;
    }

    // Load image data from memory using ImageLoader
    ImageData imageData = ImageLoader::LoadImageFromMemory(buffer, bufferSize, options);
    if (!imageData.IsValid()) {
        std::cerr << "TextureManager::LoadFromMemory failed to load image from memory: " << debugName << std::endl;
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
    textureData->samplerSettings = options.samplerSettings;

    // No source path for memory-based textures (can't hot reload)
    // But store debugName for identification
    textureData->sourcePaths.push_back(debugName);

    // Set anisotropy level
    if (HasFlag(options.flags, TextureFlags::AnisotropyOverride)) {
        textureData->anisotropyLevel = options.anisotropyLevel;
    } else {
        textureData->anisotropyLevel = 0;  // Will use global default
    }

    // Set mipmap generation policy and quality
    if (options.overrideMipmapPolicy) {
        textureData->mipmapPolicy = options.mipmapPolicy;
    }

    if (options.overrideQualityHint) {
        textureData->qualityHint = options.qualityHint;
    } else {
        textureData->qualityHint = TextureConfig::GetDefaultMipmapQuality();
    }

    // Calculate mip levels if GenerateMipmaps flag is set
    if (HasFlag(options.flags, TextureFlags::GenerateMipmaps)) {
        u32 maxDim = std::max(imageData.width, imageData.height);
        textureData->mipLevels = static_cast<u32>(std::floor(std::log2(maxDim))) + 1;
    } else {
        textureData->mipLevels = 1;
    }

    // Add to resource manager (cannot use path-based caching for memory-loaded textures)
    TextureHandle handle = Create(std::move(textureData));

    return handle;
}

TextureHandle TextureManager::LoadArray(
    const std::vector<std::string>& layerPaths,
    const TextureLoadOptions& options)
{
    // Validate input
    if (layerPaths.empty()) {
        std::cerr << "TextureManager::LoadArray: empty layer paths" << std::endl;
        return TextureHandle::Invalid;
    }

    // Note: Array textures are not cached by default (would need ResourceManager extension)
    // Each call creates a new texture unless the exact same vector is reused by caller

    // Load all layers using ImageLoader
    std::vector<ImageData> layers = ImageLoader::LoadImageArray(layerPaths, options);
    if (layers.empty()) {
        std::cerr << "TextureManager::LoadArray: failed to load layer images" << std::endl;
        return TextureHandle::Invalid;
    }

    // Create TextureData
    auto textureData = std::make_unique<TextureData>();
    textureData->width = layers[0].width;
    textureData->height = layers[0].height;
    textureData->channels = layers[0].channels;
    textureData->arrayLayers = static_cast<u32>(layers.size());
    textureData->usage = options.usage;
    textureData->type = TextureType::TextureArray;
    textureData->formatOverride = options.formatOverride;
    textureData->flags = options.flags;
    textureData->compressionHint = options.compressionHint;
    textureData->samplerSettings = options.samplerSettings;

    // Store source paths for hot reload
    textureData->sourcePaths = layerPaths;

    // Set anisotropy level (DEPRECATED)
    if (HasFlag(options.flags, TextureFlags::AnisotropyOverride)) {
        textureData->anisotropyLevel = options.anisotropyLevel;
    } else {
        textureData->anisotropyLevel = 0;
    }

    // Set mipmap generation policy and quality
    if (options.overrideMipmapPolicy) {
        textureData->mipmapPolicy = options.mipmapPolicy;
    }

    if (options.overrideQualityHint) {
        textureData->qualityHint = options.qualityHint;
    } else {
        textureData->qualityHint = TextureConfig::GetDefaultMipmapQuality();
    }

    // Calculate mip levels if GenerateMipmaps flag is set
    if (HasFlag(options.flags, TextureFlags::GenerateMipmaps)) {
        u32 maxDim = std::max(textureData->width, textureData->height);
        textureData->mipLevels = static_cast<u32>(std::floor(std::log2(maxDim))) + 1;
    } else {
        textureData->mipLevels = 1;
    }

    // Store per-layer pixel data
    textureData->layerPixels.reserve(layers.size());
    for (auto& layer : layers) {
        textureData->layerPixels.push_back(layer.pixels);
        layer.pixels = nullptr;  // Transfer ownership
    }

    // Pack layers into contiguous staging buffer
    if (!textureData->PackLayersIntoStagingBuffer()) {
        std::cerr << "TextureManager::LoadArray: failed to pack layer data" << std::endl;
        return TextureHandle::Invalid;
    }

    // Add to resource manager
    TextureHandle handle = Create(std::move(textureData));

    return handle;
}

TextureHandle TextureManager::LoadArrayPattern(
    const std::string& pathPattern,
    u32 layerCount,
    const TextureLoadOptions& options)
{
    // Generate layer paths from pattern
    std::vector<std::string> layerPaths;
    layerPaths.reserve(layerCount);
    size_t placeholderPos = pathPattern.find("{}");
    if (placeholderPos == std::string::npos) {
        std::cerr << "TextureManager::LoadArrayPattern: pattern must contain '{}'" << std::endl;
        return TextureHandle::Invalid;
    }

    for (u32 i = 0; i < layerCount; ++i) {
        std::string path = pathPattern;
        path.replace(placeholderPos, 2, std::to_string(i));
        layerPaths.push_back(path);
    }

    // Use LoadArray implementation
    return LoadArray(layerPaths, options);
}

// ============================================================================
// Cubemap Loading
// ============================================================================

TextureHandle TextureManager::LoadCubemap(
    const std::vector<std::string>& facePaths,
    const TextureLoadOptions& options)
{
    if (facePaths.size() != 6) {
        std::cerr << "TextureManager::LoadCubemap: expected 6 face paths, got " << facePaths.size() << std::endl;
        return TextureHandle::Invalid;
    }

    // Load all 6 faces using ImageLoader
    std::vector<ImageData> faces = ImageLoader::LoadCubemap(facePaths, options);
    if (faces.empty()) {
        return TextureHandle::Invalid;
    }

    // Create TextureData for cubemap
    auto textureData = std::make_unique<TextureData>();
    textureData->width = faces[0].width;
    textureData->height = faces[0].height;
    textureData->channels = faces[0].channels;
    textureData->arrayLayers = 6;  // Cubemaps always have 6 faces
    textureData->usage = options.usage;
    textureData->type = TextureType::Cubemap;
    textureData->formatOverride = options.formatOverride;
    textureData->flags = options.flags;
    textureData->compressionHint = options.compressionHint;
    textureData->samplerSettings = options.samplerSettings;

    // Store source paths for hot reload
    textureData->sourcePaths = facePaths;

    // Set anisotropy level (DEPRECATED)
    if (HasFlag(options.flags, TextureFlags::AnisotropyOverride)) {
        textureData->anisotropyLevel = options.anisotropyLevel;
    } else {
        textureData->anisotropyLevel = 0;
    }

    // Set mipmap generation policy and quality
    if (options.overrideMipmapPolicy) {
        textureData->mipmapPolicy = options.mipmapPolicy;
    }

    if (options.overrideQualityHint) {
        textureData->qualityHint = options.qualityHint;
    } else {
        textureData->qualityHint = TextureConfig::GetDefaultMipmapQuality();
    }

    // Calculate mip levels if needed
    if (HasFlag(options.flags, TextureFlags::GenerateMipmaps)) {
        u32 maxDim = std::max(textureData->width, textureData->height);
        textureData->mipLevels = static_cast<u32>(std::floor(std::log2(maxDim))) + 1;
    } else {
        textureData->mipLevels = 1;
    }

    // Transfer face pixel data to layerPixels
    textureData->layerPixels.reserve(6);
    for (auto& face : faces) {
        textureData->layerPixels.push_back(face.pixels);
        face.pixels = nullptr;  // Transfer ownership
    }

    // Pack layers into contiguous staging buffer
    if (!textureData->PackLayersIntoStagingBuffer()) {
        std::cerr << "TextureManager::LoadCubemap: failed to pack face data" << std::endl;
        return TextureHandle::Invalid;
    }

    // Validate cubemap structure
    if (!textureData->ValidateCubemap()) {
        std::cerr << "TextureManager::LoadCubemap: cubemap validation failed" << std::endl;
        return TextureHandle::Invalid;
    }

    return Create(std::move(textureData));
}

TextureHandle TextureManager::LoadCubemapPattern(
    const std::string& pathPattern,
    const TextureLoadOptions& options)
{
    // Validate pattern contains "{}"
    size_t placeholderPos = pathPattern.find("{}");
    if (placeholderPos == std::string::npos) {
        std::cerr << "TextureManager::LoadCubemapPattern: pattern must contain '{}'" << std::endl;
        return TextureHandle::Invalid;
    }

    // Generate face paths using pattern
    const char* faceNames[6] = {"px", "nx", "py", "ny", "pz", "nz"};
    std::vector<std::string> facePaths;
    facePaths.reserve(6);

    for (u32 i = 0; i < 6; ++i) {
        std::string path = pathPattern;
        path.replace(placeholderPos, 2, faceNames[i]);
        facePaths.push_back(path);
    }

    // Use LoadCubemap implementation
    return LoadCubemap(facePaths, options);
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

    // Set mipmap policy (use defaults for LoadResource)
    if (m_DefaultOptions.overrideQualityHint) {
        textureData->qualityHint = m_DefaultOptions.qualityHint;
    } else {
        textureData->qualityHint = TextureConfig::GetDefaultMipmapQuality();
    }
    // mipmapPolicy already initialized to Auto in TextureData constructor

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

TextureHandle TextureManager::CreateMetalRough() {
    if (!IsValid(m_MetalRoughTexture)) {
        // Default metal-roughness texture: R=roughness (1.0), G=metalness (0.5), B=unused (0)
        // R=255 (fully rough), G=128 (mid metallic), B=0, A=255
        m_MetalRoughTexture = CreateSinglePixel(255, 128, 0, 255, TextureUsage::PackedPBR);
    }
    return m_MetalRoughTexture;
}

TextureHandle TextureManager::CreateCheckerboard(u32 size, const Vec4& color1, const Vec4& color2) {
    auto textureData = std::make_unique<TextureData>();
    textureData->width = size;
    textureData->height = size;
    textureData->channels = 4;  // RGBA
    textureData->usage = TextureUsage::Generic;
    textureData->type = TextureType::Texture2D;
    textureData->flags = TextureFlags::None;  // No mipmaps for debug textures
    textureData->mipLevels = 1;

    // Allocate pixel data
    const u32 pixelCount = size * size;
    const u32 dataSize = pixelCount * 4;
    textureData->pixels = static_cast<u8*>(malloc(dataSize));

    // Convert colors from [0,1] to [0,255]
    u8 r1 = static_cast<u8>(color1.r * 255.0f);
    u8 g1 = static_cast<u8>(color1.g * 255.0f);
    u8 b1 = static_cast<u8>(color1.b * 255.0f);
    u8 a1 = static_cast<u8>(color1.a * 255.0f);

    u8 r2 = static_cast<u8>(color2.r * 255.0f);
    u8 g2 = static_cast<u8>(color2.g * 255.0f);
    u8 b2 = static_cast<u8>(color2.b * 255.0f);
    u8 a2 = static_cast<u8>(color2.a * 255.0f);

    // Create 2x2 checkerboard pattern
    const u32 checkerSize = size / 2;
    for (u32 y = 0; y < size; ++y) {
        for (u32 x = 0; x < size; ++x) {
            u32 idx = (y * size + x) * 4;

            // Determine which checker we're in (alternates every checkerSize pixels)
            bool useColor1 = ((x / checkerSize) + (y / checkerSize)) % 2 == 0;

            if (useColor1) {
                textureData->pixels[idx + 0] = r1;
                textureData->pixels[idx + 1] = g1;
                textureData->pixels[idx + 2] = b1;
                textureData->pixels[idx + 3] = a1;
            } else {
                textureData->pixels[idx + 0] = r2;
                textureData->pixels[idx + 1] = g2;
                textureData->pixels[idx + 2] = b2;
                textureData->pixels[idx + 3] = a2;
            }
        }
    }

    return Create(std::move(textureData));
}

// ============================================================================
// Asynchronous Loading Implementation
// ============================================================================

// Forward declaration of worker function
static void TextureLoadWorker(void* data);

TextureHandle TextureManager::LoadAsync(
    const std::string& filepath,
    const TextureLoadOptions& options,
    AsyncLoadCallback callback,
    void* userData)
{
    // Check if already loaded (cache hit)
    TextureHandle existing = GetHandle(filepath);
    if (IsValid(existing)) {
        // Already loaded - invoke callback immediately with success
        if (callback) {
            callback(existing, true, userData);
        }
        return existing;
    }

    // Allocate handle immediately and assign placeholder texture
    auto placeholderData = std::make_unique<TextureData>();

    // Get white placeholder texture data
    TextureData* whiteTexture = Get(CreateWhite());
    if (whiteTexture) {
        // Copy placeholder data (shallow copy for GPU texture pointer)
        placeholderData->width = whiteTexture->width;
        placeholderData->height = whiteTexture->height;
        placeholderData->channels = whiteTexture->channels;
        placeholderData->usage = options.usage;
        placeholderData->type = options.type;
        placeholderData->flags = whiteTexture->flags;
        placeholderData->mipLevels = whiteTexture->mipLevels;

        // Share GPU texture pointer (both point to same white texture)
        placeholderData->gpuTexture = whiteTexture->gpuTexture;
        placeholderData->gpuUploaded = whiteTexture->gpuUploaded;

        // Don't allocate pixels - we'll replace this data when async load completes
        placeholderData->pixels = nullptr;
    }

    // Create handle via thread-safe Create()
    TextureHandle handle = Create(std::move(placeholderData));

    // Update path maps (already thread-safe via ResourceManager mutex)
    Platform::Lock(m_AsyncMutex.get());
    // Note: ResourceManager::Create doesn't add to path maps, so we do it manually here
    // This is temporary - will be updated when actual texture loads
    Platform::Unlock(m_AsyncMutex.get());

    // Allocate TextureLoadJob from pool
    TextureLoadJob* loadJob = m_JobAllocator.Alloc();
    if (!loadJob) {
        std::cerr << "TextureManager::LoadAsync failed to allocate job for: " << filepath << std::endl;

        // Invoke callback with failure
        if (callback) {
            callback(handle, false, userData);
        }

        return handle;
    }

    // Initialize job data
    loadJob->filepath = filepath;
    loadJob->options = options;
    loadJob->handle = handle;
    loadJob->userData = userData;
    loadJob->callback = callback;
    loadJob->state.store(AsyncLoadState::Pending);
    loadJob->errorMessage.clear();

    // Create JobSystem job
    loadJob->job = JobSystem::CreateJob(TextureLoadWorker, loadJob, JobSystem::JobPriority::Normal);

    if (!loadJob->job) {
        std::cerr << "TextureManager::LoadAsync failed to create JobSystem job for: " << filepath << std::endl;
        loadJob->state.store(AsyncLoadState::Failed);
        loadJob->errorMessage = "Failed to create JobSystem job";

        // Cleanup
        m_JobAllocator.Free(loadJob);

        // Invoke callback with failure
        if (callback) {
            callback(handle, false, userData);
        }

        return handle;
    }

    // Run job on JobSystem
    JobSystem::Run(loadJob->job);

    return handle;
}

// Forward declaration of array texture worker function
static void TextureLoadWorkerArray(void* data);

TextureHandle TextureManager::LoadArrayAsync(
    const std::vector<std::string>& layerPaths,
    const TextureLoadOptions& options,
    AsyncLoadCallback callback,
    void* userData)
{
    // Validate input
    if (layerPaths.empty()) {
        std::cerr << "TextureManager::LoadArrayAsync: empty layer paths" << std::endl;
        return TextureHandle::Invalid;
    }

    // Note: Array textures don't use cache checking for now
    // Create placeholder handle immediately
    auto placeholderData = std::make_unique<TextureData>();

    // Get white placeholder texture data
    TextureData* whiteTexture = Get(CreateWhite());
    if (whiteTexture) {
        // Copy placeholder data (shallow copy for GPU texture pointer)
        placeholderData->width = whiteTexture->width;
        placeholderData->height = whiteTexture->height;
        placeholderData->channels = whiteTexture->channels;
        placeholderData->usage = options.usage;
        placeholderData->type = TextureType::TextureArray;
        placeholderData->flags = whiteTexture->flags;
        placeholderData->mipLevels = whiteTexture->mipLevels;
        placeholderData->arrayLayers = static_cast<u32>(layerPaths.size());

        // Share GPU texture pointer (both point to same white texture temporarily)
        placeholderData->gpuTexture = whiteTexture->gpuTexture;
        placeholderData->gpuUploaded = whiteTexture->gpuUploaded;

        // Don't allocate pixels - we'll replace this data when async load completes
        placeholderData->pixels = nullptr;
    }

    // Create handle via thread-safe Create()
    TextureHandle handle = Create(std::move(placeholderData));

    // Allocate TextureLoadJob from pool
    TextureLoadJob* loadJob = m_JobAllocator.Alloc();
    if (!loadJob) {
        std::cerr << "TextureManager::LoadArrayAsync failed to allocate job" << std::endl;

        // Invoke callback with failure
        if (callback) {
            callback(handle, false, userData);
        }

        return handle;
    }

    // Initialize job data for array texture
    loadJob->isArrayTexture = true;
    loadJob->layerPaths = layerPaths;
    loadJob->options = options;
    loadJob->handle = handle;
    loadJob->userData = userData;
    loadJob->callback = callback;
    loadJob->state.store(AsyncLoadState::Pending);
    loadJob->errorMessage.clear();

    // Create JobSystem job
    loadJob->job = JobSystem::CreateJob(TextureLoadWorkerArray, loadJob, JobSystem::JobPriority::Normal);

    if (!loadJob->job) {
        std::cerr << "TextureManager::LoadArrayAsync failed to create JobSystem job" << std::endl;
        loadJob->state.store(AsyncLoadState::Failed);
        loadJob->errorMessage = "Failed to create JobSystem job";

        // Cleanup
        m_JobAllocator.Free(loadJob);

        // Invoke callback with failure
        if (callback) {
            callback(handle, false, userData);
        }

        return handle;
    }

    // Run job on JobSystem
    JobSystem::Run(loadJob->job);

    return handle;
}

void TextureManager::EnqueuePendingUpload(TextureLoadJob* job) {
    Platform::Lock(m_AsyncMutex.get());
    m_PendingUploads.push_back(job);
    Platform::Unlock(m_AsyncMutex.get());
}

void TextureManager::ProcessUploads(u64 maxBytesPerFrame) {
    // Only process if async pipeline is initialized
    if (!m_VulkanContext || !m_TransferQueue || !m_StagingPool) {
        return;
    }

    // Swap pending queue to local vector (minimize lock time)
    std::vector<TextureLoadJob*> uploads;
    {
        Platform::Lock(m_AsyncMutex.get());
        uploads.swap(m_PendingUploads);
        Platform::Unlock(m_AsyncMutex.get());
    }

    u64 bytesUploadedThisFrame = 0;

    // Process uploads with bandwidth limiting
    for (TextureLoadJob* job : uploads) {
        AsyncLoadState state = job->state.load();

        if (state != AsyncLoadState::ReadyForUpload) {
            if (state == AsyncLoadState::Failed) {
                ProcessFailure(job);
            }
            continue;
        }

        // Calculate upload size
        u64 uploadSize = 0;
        if (job->isArrayTexture) {
            for (const auto& layer : job->layerImageData) {
                uploadSize += layer.width * layer.height * layer.channels;
            }
        } else {
            uploadSize = job->imageData.width * job->imageData.height * job->imageData.channels;
        }

        // Check bandwidth limit
        if (bytesUploadedThisFrame + uploadSize > maxBytesPerFrame && bytesUploadedThisFrame > 0) {
            // Re-queue this job for next frame
            Platform::Lock(m_AsyncMutex.get());
            m_PendingUploads.push_back(job);
            Platform::Unlock(m_AsyncMutex.get());
            continue;
        }

        // Process the upload asynchronously
        job->state.store(AsyncLoadState::Uploading);

        // Create TextureData from ImageData
        auto textureData = std::make_unique<TextureData>();

        if (job->isArrayTexture) {
            // Array texture setup
            if (job->layerImageData.empty()) {
                std::cerr << "TextureManager::ProcessUploads: array texture has no layer data" << std::endl;
                job->state.store(AsyncLoadState::Failed);
                ProcessFailure(job);
                continue;
            }

            textureData->width = job->layerImageData[0].width;
            textureData->height = job->layerImageData[0].height;
            textureData->channels = job->layerImageData[0].channels;
            textureData->arrayLayers = static_cast<u32>(job->layerImageData.size());
            textureData->type = TextureType::TextureArray;

            // Combine all layer data into a single buffer
            u64 layerSize = textureData->width * textureData->height * textureData->channels;
            u64 totalSize = layerSize * textureData->arrayLayers;
            textureData->pixels = new u8[totalSize];

            for (u32 i = 0; i < textureData->arrayLayers; ++i) {
                memcpy(textureData->pixels + (i * layerSize), job->layerImageData[i].pixels, layerSize);
            }
        } else {
            // Single texture
            textureData->pixels = job->imageData.pixels;
            textureData->width = job->imageData.width;
            textureData->height = job->imageData.height;
            textureData->channels = job->imageData.channels;
            textureData->arrayLayers = 1;
            textureData->type = TextureType::Texture2D;
        }

        // Copy texture options
        textureData->usage = job->options.usage;
        textureData->formatOverride = job->options.formatOverride;
        textureData->flags = job->options.flags;
        textureData->compressionHint = job->options.compressionHint;
        textureData->samplerSettings = job->options.samplerSettings;
        textureData->mipmapPolicy = job->options.mipmapPolicy;
        textureData->qualityHint = job->options.qualityHint;

        // Get the texture data
        TextureData* texData = Get(job->handle);
        if (!texData) {
            std::cerr << "TextureManager::ProcessUploads: texture handle is invalid" << std::endl;
            job->state.store(AsyncLoadState::Failed);
            ProcessFailure(job);
            if (job->isArrayTexture) {
                delete[] textureData->pixels;
            }
            continue;
        }

        // Create VulkanTexture asynchronously
        VulkanTexture* vulkanTex = static_cast<VulkanTexture*>(texData->gpuTexture);
        if (!vulkanTex) {
            vulkanTex = new VulkanTexture();
            texData->gpuTexture = vulkanTex;
        }

        // Initiate async GPU upload
        try {
            vulkanTex->CreateAsync(
                m_VulkanContext,
                m_TransferQueue,
                m_StagingPool,
                textureData.get(),
                [this, job, uploadSize](bool success) {
                    if (!success) {
                        std::cerr << "TextureManager: Async GPU upload failed" << std::endl;
                        job->state.store(AsyncLoadState::Failed);
                        ProcessFailure(job);
                    }
                }
            );

            // Get timeline value from the transfer queue (we'll need to track this)
            u64 timelineValue = m_VulkanContext->GetCurrentTransferTimelineValue();

            // Add to GPU upload queue
            Platform::Lock(m_GPUUploadMutex.get());
            m_GPUUploadQueue.push_back({
                job->handle,
                timelineValue,
                job->callback,
                job->userData,
                uploadSize
            });
            Platform::Unlock(m_GPUUploadMutex.get());

            bytesUploadedThisFrame += uploadSize;
        } catch (const std::exception& e) {
            std::cerr << "TextureManager::ProcessUploads exception: " << e.what() << std::endl;
            job->state.store(AsyncLoadState::Failed);
            ProcessFailure(job);
        }

        // Clean up temporary texture data
        if (job->isArrayTexture) {
            delete[] textureData->pixels;
        }
    }
}

void TextureManager::Update() {
    // Update file watcher for hot reload
    if (m_HotReloadEnabled) {
        m_FileWatcher.Update();
    }

    // First, check for completed GPU uploads
    if (m_VulkanContext && m_TransferQueue) {
        Platform::Lock(m_GPUUploadMutex.get());
        auto it = m_GPUUploadQueue.begin();
        while (it != m_GPUUploadQueue.end()) {
            if (m_TransferQueue->IsTransferComplete(it->timelineValue)) {
                // GPU upload is complete, finish the texture creation
                TextureData* texData = Get(it->handle);
                if (texData && texData->gpuTexture) {
                    VulkanTexture* vulkanTex = static_cast<VulkanTexture*>(texData->gpuTexture);
                    if (vulkanTex->FinishAsyncCreation()) {
                        // Invoke callback on success
                        if (it->callback) {
                            it->callback(it->handle, true, it->userData);
                        }
                    } else {
                        // Failed to finish creation
                        if (it->callback) {
                            it->callback(it->handle, false, it->userData);
                        }
                    }
                }

                // Remove from queue
                it = m_GPUUploadQueue.erase(it);
            } else {
                ++it;
            }
        }
        Platform::Unlock(m_GPUUploadMutex.get());
    }

    // Then process new uploads with default bandwidth limit
    ProcessUploads();
}

void TextureManager::ProcessUpload(TextureLoadJob* job) {
    job->state.store(AsyncLoadState::Uploading);

    // Create TextureData from ImageData (handle both single and array textures)
    auto textureData = std::make_unique<TextureData>();

    if (job->isArrayTexture) {
        // Array texture loading
        if (job->layerImageData.empty()) {
            std::cerr << "TextureManager::ProcessUpload: array texture has no layer data" << std::endl;
            job->state.store(AsyncLoadState::Failed);
            ProcessFailure(job);
            return;
        }

        textureData->width = job->layerImageData[0].width;
        textureData->height = job->layerImageData[0].height;
        textureData->channels = job->layerImageData[0].channels;
        textureData->arrayLayers = static_cast<u32>(job->layerImageData.size());
        textureData->type = TextureType::TextureArray;

        // Transfer layer pixel data
        textureData->layerPixels.reserve(job->layerImageData.size());
        for (auto& layer : job->layerImageData) {
            textureData->layerPixels.push_back(layer.pixels);
            layer.pixels = nullptr;  // Transfer ownership
        }

        // Pack layers into contiguous buffer
        if (!textureData->PackLayersIntoStagingBuffer()) {
            std::cerr << "TextureManager::ProcessUpload: failed to pack array texture layers" << std::endl;
            job->state.store(AsyncLoadState::Failed);
            ProcessFailure(job);
            return;
        }
    } else {
        // Single texture loading
        textureData->pixels = job->imageData.pixels;
        textureData->width = job->imageData.width;
        textureData->height = job->imageData.height;
        textureData->channels = job->imageData.channels;
        textureData->type = job->options.type;
        textureData->arrayLayers = 1;
    }

    textureData->usage = job->options.usage;
    textureData->formatOverride = job->options.formatOverride;
    textureData->flags = job->options.flags;
    textureData->compressionHint = job->options.compressionHint;

    // Set anisotropy level
    if (HasFlag(job->options.flags, TextureFlags::AnisotropyOverride)) {
        textureData->anisotropyLevel = job->options.anisotropyLevel;
    } else {
        textureData->anisotropyLevel = 0;  // Use global default
    }

    // Set mipmap generation policy and quality
    if (job->options.overrideMipmapPolicy) {
        textureData->mipmapPolicy = job->options.mipmapPolicy;
    }
    // Note: mipmapPolicy already initialized to Auto in TextureData constructor

    if (job->options.overrideQualityHint) {
        textureData->qualityHint = job->options.qualityHint;
    } else {
        textureData->qualityHint = TextureConfig::GetDefaultMipmapQuality();
    }

    // Calculate mip levels
    if (HasFlag(job->options.flags, TextureFlags::GenerateMipmaps)) {
        u32 maxDim = std::max(job->imageData.width, job->imageData.height);
        textureData->mipLevels = static_cast<u32>(std::floor(std::log2(maxDim))) + 1;
    } else {
        textureData->mipLevels = 1;
    }

    // Replace placeholder with real data
    TextureData* existing = Get(job->handle);
    if (existing) {
        // Move texture data into existing slot
        *existing = std::move(*textureData);
        existing->gpuUploaded = false;  // Mark for GPU upload by renderer
    }

    // Clear ImageData (ownership transferred to TextureData)
    job->imageData.pixels = nullptr;

    job->state.store(AsyncLoadState::Completed);

    // Invoke callback with success
    if (job->callback) {
        job->callback(job->handle, true, job->userData);
    }

    // Cleanup job
    m_JobAllocator.Free(job);
}

void TextureManager::ProcessFailure(TextureLoadJob* job) {
    std::cerr << "TextureManager async load failed: " << job->filepath;
    if (!job->errorMessage.empty()) {
        std::cerr << " - " << job->errorMessage;
    }
    std::cerr << std::endl;

    // Keep placeholder texture (already assigned to handle)
    // The handle remains valid and points to white texture

    // Invoke callback with failure
    if (job->callback) {
        job->callback(job->handle, false, job->userData);
    }

    // Cleanup job (don't free ImageData as it should be invalid)
    m_JobAllocator.Free(job);
}

// ============================================================================
// Worker Thread Function
// ============================================================================

static void TextureLoadWorker(void* data) {
    TextureLoadJob* job = static_cast<TextureLoadJob*>(data);
    job->state.store(AsyncLoadState::Loading);

    // File I/O and image decoding (blocking, on worker thread)
    job->imageData = ImageLoader::LoadImage(job->filepath, job->options);

    if (!job->imageData.IsValid()) {
        job->state.store(AsyncLoadState::Failed);
        job->errorMessage = "Failed to load image from file";

        // Enqueue for failure processing on main thread
        TextureManager::Instance().EnqueuePendingUpload(job);
        return;
    }

    // Successfully loaded - ready for GPU upload
    job->state.store(AsyncLoadState::ReadyForUpload);

    // Enqueue for GPU upload on main thread
    TextureManager::Instance().EnqueuePendingUpload(job);
}

static void TextureLoadWorkerArray(void* data) {
    TextureLoadJob* job = static_cast<TextureLoadJob*>(data);
    job->state.store(AsyncLoadState::Loading);

    // Load all array layers (blocking, on worker thread)
    job->layerImageData = ImageLoader::LoadImageArray(job->layerPaths, job->options);

    if (job->layerImageData.empty()) {
        job->state.store(AsyncLoadState::Failed);
        job->errorMessage = "Failed to load array texture layers";
        TextureManager::Instance().EnqueuePendingUpload(job);
        return;
    }

    // Successfully loaded - ready for GPU upload
    job->state.store(AsyncLoadState::ReadyForUpload);
    TextureManager::Instance().EnqueuePendingUpload(job);
}

// ============================================================================
// Hot Reload Implementation
// ============================================================================

void TextureManager::EnableHotReload(bool enable) {
    m_HotReloadEnabled = enable;

    if (enable) {
        // Register callbacks for all supported image extensions
        const std::vector<std::string> extensions = {
            ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".hdr", ".exr"
        };

        for (const auto& ext : extensions) {
            m_FileWatcher.RegisterCallback(ext, [this](const std::string& filepath, FileAction action) {
                this->OnTextureFileModified(filepath, action);
            });
        }

        std::cout << "TextureManager: Hot reload enabled" << std::endl;
    } else {
        std::cout << "TextureManager: Hot reload disabled" << std::endl;
    }
}

void TextureManager::WatchDirectory(const std::string& directory, bool recursive) {
    if (!m_HotReloadEnabled) {
        std::cerr << "TextureManager::WatchDirectory: hot reload is not enabled, call EnableHotReload(true) first" << std::endl;
        return;
    }

    m_FileWatcher.WatchDirectory(directory, recursive);
    m_WatchedDirectories.insert(directory);

    std::cout << "TextureManager: Watching directory for texture changes: " << directory << std::endl;
}

void TextureManager::OnTextureFileModified(const std::string& filepath, FileAction action) {
    if (action == FileAction::Modified) {
        std::cout << "TextureManager: Detected modification of " << filepath << ", reloading..." << std::endl;
        ReloadTexture(filepath);
    } else if (action == FileAction::Deleted) {
        std::cout << "TextureManager: Texture file deleted: " << filepath << std::endl;
        // Optionally replace with error texture
        TextureHandle handle = GetHandle(filepath);
        if (IsValid(handle)) {
            TextureData* texData = Get(handle);
            if (texData && texData->gpuTexture) {
                // Replace with error texture's GPU data
                TextureData* errorTex = Get(m_ErrorTexture);
                if (errorTex && errorTex->gpuTexture) {
                    texData->gpuTexture = errorTex->gpuTexture;
                }
            }
        }
    }
}

bool TextureManager::ReloadTexture(const std::string& filepath) {
    // Get handle from filepath
    TextureHandle handle = GetHandle(filepath);
    if (!IsValid(handle)) {
        std::cerr << "TextureManager::ReloadTexture: no texture loaded from " << filepath << std::endl;
        return false;
    }

    return ReloadTexture(handle);
}

bool TextureManager::ReloadTexture(TextureHandle handle) {
    TextureData* texData = Get(handle);
    if (!texData) {
        std::cerr << "TextureManager::ReloadTexture: invalid handle" << std::endl;
        return false;
    }

    // Get filepath from handle (reverse lookup)
    std::string filepath = GetPath(handle);
    if (filepath.empty()) {
        std::cerr << "TextureManager::ReloadTexture: cannot find filepath for handle" << std::endl;
        return false;
    }

    std::cout << "TextureManager: Reloading texture from " << filepath << std::endl;

    // Perform async reload
    PerformReload(handle, filepath);

    return true;
}

void TextureManager::PerformReload(TextureHandle handle, const std::string& filepath) {
    TextureData* texData = Get(handle);
    if (!texData) {
        return;
    }

    // Store original descriptor index (critical for preserving descriptor bindings)
    u32 oldDescriptorIndex = 0xFFFFFFFF;
    if (texData->gpuTexture) {
        VulkanTexture* vulkanTex = static_cast<VulkanTexture*>(texData->gpuTexture);
        oldDescriptorIndex = vulkanTex->GetDescriptorIndex();
    }

    // Store original texture type and layer info for reload
    TextureType originalType = texData->type;

    // Replace GPU texture with placeholder temporarily
    TextureData* placeholderData = Get(m_PlaceholderTexture);
    if (placeholderData && placeholderData->gpuTexture) {
        // NOTE: We don't actually destroy the old texture yet,
        // we'll do that after successful reload
        texData->gpuTexture = placeholderData->gpuTexture;
    }

    // Build load options from existing texture data
    TextureLoadOptions loadOptions;
    loadOptions.usage = texData->usage;
    loadOptions.type = texData->type;
    loadOptions.formatOverride = texData->formatOverride;
    loadOptions.flags = texData->flags;
    loadOptions.compressionHint = texData->compressionHint;
    loadOptions.samplerSettings = texData->samplerSettings;
    loadOptions.mipmapPolicy = texData->mipmapPolicy;
    loadOptions.qualityHint = texData->qualityHint;
    loadOptions.overrideMipmapPolicy = true;
    loadOptions.overrideQualityHint = true;

    // Reload based on texture type
    if (originalType == TextureType::TextureArray) {
        // Check if we have source paths stored
        if (texData->sourcePaths.empty()) {
            std::cerr << "TextureManager::PerformReload: array texture has no source paths stored" << std::endl;

            // Replace with error texture
            TextureData* errorData = Get(m_ErrorTexture);
            if (errorData && errorData->gpuTexture) {
                texData->gpuTexture = errorData->gpuTexture;
            }
            return;
        }

        // Reload all array layers
        std::vector<ImageData> layers = ImageLoader::LoadImageArray(texData->sourcePaths, loadOptions);
        if (layers.empty()) {
            std::cerr << "TextureManager::PerformReload: failed to reload array texture" << std::endl;

            // Replace with error texture
            TextureData* errorData = Get(m_ErrorTexture);
            if (errorData && errorData->gpuTexture) {
                texData->gpuTexture = errorData->gpuTexture;
            }
            return;
        }

        // Update texture data
        if (texData->pixels) {
            free(texData->pixels);
        }

        // Clear old layer pixels
        for (auto* layerPtr : texData->layerPixels) {
            free(layerPtr);
        }
        texData->layerPixels.clear();

        // Transfer new layer pixel data
        texData->width = layers[0].width;
        texData->height = layers[0].height;
        texData->channels = layers[0].channels;
        texData->arrayLayers = static_cast<u32>(layers.size());

        texData->layerPixels.reserve(layers.size());
        for (auto& layer : layers) {
            texData->layerPixels.push_back(layer.pixels);
            layer.pixels = nullptr;  // Transfer ownership
        }

        // Pack layers into contiguous buffer
        if (!texData->PackLayersIntoStagingBuffer()) {
            std::cerr << "TextureManager::PerformReload: failed to pack array texture layers" << std::endl;

            // Replace with error texture
            TextureData* errorData = Get(m_ErrorTexture);
            if (errorData && errorData->gpuTexture) {
                texData->gpuTexture = errorData->gpuTexture;
            }
            return;
        }

    } else if (originalType == TextureType::Cubemap) {
        // Check if we have source paths stored
        if (texData->sourcePaths.size() != 6) {
            std::cerr << "TextureManager::PerformReload: cubemap doesn't have 6 face paths stored" << std::endl;

            // Replace with error texture
            TextureData* errorData = Get(m_ErrorTexture);
            if (errorData && errorData->gpuTexture) {
                texData->gpuTexture = errorData->gpuTexture;
            }
            return;
        }

        // Reload all 6 cubemap faces
        std::vector<ImageData> faces = ImageLoader::LoadCubemap(texData->sourcePaths, loadOptions);
        if (faces.empty()) {
            std::cerr << "TextureManager::PerformReload: failed to reload cubemap" << std::endl;

            // Replace with error texture
            TextureData* errorData = Get(m_ErrorTexture);
            if (errorData && errorData->gpuTexture) {
                texData->gpuTexture = errorData->gpuTexture;
            }
            return;
        }

        // Update texture data
        if (texData->pixels) {
            free(texData->pixels);
        }

        // Clear old layer pixels
        for (auto* layerPtr : texData->layerPixels) {
            free(layerPtr);
        }
        texData->layerPixels.clear();

        // Transfer new face pixel data
        texData->width = faces[0].width;
        texData->height = faces[0].height;
        texData->channels = faces[0].channels;
        texData->arrayLayers = 6;

        texData->layerPixels.reserve(6);
        for (auto& face : faces) {
            texData->layerPixels.push_back(face.pixels);
            face.pixels = nullptr;  // Transfer ownership
        }

        // Pack layers into contiguous buffer
        if (!texData->PackLayersIntoStagingBuffer()) {
            std::cerr << "TextureManager::PerformReload: failed to pack cubemap faces" << std::endl;

            // Replace with error texture
            TextureData* errorData = Get(m_ErrorTexture);
            if (errorData && errorData->gpuTexture) {
                texData->gpuTexture = errorData->gpuTexture;
            }
            return;
        }

        // Validate cubemap structure
        if (!texData->ValidateCubemap()) {
            std::cerr << "TextureManager::PerformReload: cubemap validation failed" << std::endl;

            // Replace with error texture
            TextureData* errorData = Get(m_ErrorTexture);
            if (errorData && errorData->gpuTexture) {
                texData->gpuTexture = errorData->gpuTexture;
            }
            return;
        }

    } else {
        // Single 2D texture reload
        ImageData imageData = ImageLoader::LoadImage(filepath, loadOptions);
        if (!imageData.IsValid()) {
            std::cerr << "TextureManager::PerformReload: failed to load image from " << filepath << std::endl;

            // Replace with error texture
            TextureData* errorData = Get(m_ErrorTexture);
            if (errorData && errorData->gpuTexture) {
                texData->gpuTexture = errorData->gpuTexture;
            }
            return;
        }

        // Update texture data
        if (texData->pixels) {
            free(texData->pixels);
        }
        texData->pixels = imageData.pixels;
        texData->width = imageData.width;
        texData->height = imageData.height;
        texData->channels = imageData.channels;

        // Recalculate mip levels if needed
        if (HasFlag(texData->flags, TextureFlags::GenerateMipmaps)) {
            u32 maxDim = std::max(imageData.width, imageData.height);
            texData->mipLevels = static_cast<u32>(std::floor(std::log2(maxDim))) + 1;
        }
    }

    // Destroy old GPU texture if it exists and isn't a shared placeholder/error texture
    VulkanTexture* oldVulkanTex = static_cast<VulkanTexture*>(texData->gpuTexture);
    if (oldVulkanTex && oldDescriptorIndex != 0xFFFFFFFF) {
        // Only destroy if it's not the placeholder/error texture
        TextureData* placeholderCheck = Get(m_PlaceholderTexture);
        TextureData* errorCheck = Get(m_ErrorTexture);
        if (oldVulkanTex != placeholderCheck->gpuTexture && oldVulkanTex != errorCheck->gpuTexture) {
            oldVulkanTex->Destroy();
            delete oldVulkanTex;
        }
    }

    // Create new GPU texture with preserved descriptor index
    VulkanTexture* newVulkanTex = new VulkanTexture();
    if (oldDescriptorIndex != 0xFFFFFFFF) {
        newVulkanTex->SetDescriptorIndex(oldDescriptorIndex);
    }

    // Upload to GPU (synchronous for now, TODO: make async)
    if (m_VulkanContext) {
        newVulkanTex->Create(m_VulkanContext, texData);
        texData->gpuTexture = newVulkanTex;
        texData->gpuUploaded = true;

        // Increment generation to invalidate old references (triggers material rebuild)
        IncrementGeneration(handle);

        // Invalidate materials using this texture (triggers descriptor rebuild)
        MaterialManager::Instance().InvalidateMaterialsUsingTexture(handle);

        std::cout << "TextureManager: Successfully reloaded " << filepath << std::endl;
    } else {
        std::cerr << "TextureManager::PerformReload: VulkanContext not initialized" << std::endl;
        delete newVulkanTex;

        // Replace with error texture
        TextureData* errorData = Get(m_ErrorTexture);
        if (errorData && errorData->gpuTexture) {
            texData->gpuTexture = errorData->gpuTexture;
        }
    }
}

u32 TextureManager::GetDescriptorIndex(TextureHandle handle) const {
    const TextureData* texData = Get(handle);
    if (!texData || !texData->gpuTexture) {
        return 0;  // Return index 0 (default white texture) for invalid handles
    }

    const VulkanTexture* vulkanTex = static_cast<const VulkanTexture*>(texData->gpuTexture);
    return vulkanTex->GetDescriptorIndex();
}
