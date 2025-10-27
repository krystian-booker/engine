#include "resources/texture_manager.h"
#include "resources/image_loader.h"
#include "core/job_system.h"
#include "platform/platform.h"
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

    // Initialize async loading state
    m_AsyncMutex = Platform::CreateMutex();
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

void TextureManager::EnqueuePendingUpload(TextureLoadJob* job) {
    Platform::Lock(m_AsyncMutex.get());
    m_PendingUploads.push_back(job);
    Platform::Unlock(m_AsyncMutex.get());
}

void TextureManager::Update() {
    // Swap pending queue to local vector (minimize lock time)
    std::vector<TextureLoadJob*> uploads;
    {
        Platform::Lock(m_AsyncMutex.get());
        uploads.swap(m_PendingUploads);
        Platform::Unlock(m_AsyncMutex.get());
    }

    // Process each upload on main thread
    for (TextureLoadJob* job : uploads) {
        AsyncLoadState state = job->state.load();

        if (state == AsyncLoadState::ReadyForUpload) {
            ProcessUpload(job);
        } else if (state == AsyncLoadState::Failed) {
            ProcessFailure(job);
        }
    }
}

void TextureManager::ProcessUpload(TextureLoadJob* job) {
    job->state.store(AsyncLoadState::Uploading);

    // Create TextureData from ImageData
    auto textureData = std::make_unique<TextureData>();
    textureData->pixels = job->imageData.pixels;
    textureData->width = job->imageData.width;
    textureData->height = job->imageData.height;
    textureData->channels = job->imageData.channels;
    textureData->usage = job->options.usage;
    textureData->type = job->options.type;
    textureData->formatOverride = job->options.formatOverride;
    textureData->flags = job->options.flags;
    textureData->compressionHint = job->options.compressionHint;

    // Set anisotropy level
    if (HasFlag(job->options.flags, TextureFlags::AnisotropyOverride)) {
        textureData->anisotropyLevel = job->options.anisotropyLevel;
    } else {
        textureData->anisotropyLevel = 0;  // Use global default
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
