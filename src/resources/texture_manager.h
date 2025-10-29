#pragma once
#include "core/resource_manager.h"
#include "core/texture_data.h"
#include "core/texture_load_options.h"
#include "core/math.h"
#include "core/memory.h"
#include "core/file_watcher.h"
#include "resources/texture_load_job.h"
#include "platform/platform.h"
#include <vector>
#include <functional>
#include <unordered_set>

// Forward declarations
class VulkanContext;
class VulkanTransferQueue;
class VulkanStagingPool;
enum class MipmapQuality : u8;

// Global texture configuration
namespace TextureConfig {
    extern u32 g_DefaultAnisotropy;
    void SetDefaultAnisotropy(u32 level);
    u32 GetDefaultAnisotropy();

    extern MipmapQuality g_DefaultMipmapQuality;
    void SetDefaultMipmapQuality(MipmapQuality quality);
    MipmapQuality GetDefaultMipmapQuality();
}

// Texture resource manager (singleton)
class TextureManager : public ResourceManager<TextureData, TextureHandle> {
public:
    // Singleton access
    static TextureManager& Instance() {
        static TextureManager instance;
        return instance;
    }

    // Initialize async upload pipeline (must be called after VulkanContext is created)
    void InitAsyncPipeline(VulkanContext* context, VulkanTransferQueue* transferQueue, VulkanStagingPool* stagingPool);

    // Shutdown async upload pipeline
    void ShutdownAsyncPipeline();

    // Load texture from file with options
    TextureHandle Load(const std::string& filepath, const TextureLoadOptions& options);

    // Load with default options (inferred from file extension or usage)
    TextureHandle Load(const std::string& filepath) {
        return ResourceManager::Load(filepath);
    }

    // ========================================================================
    // Array Texture Loading
    // ========================================================================

    // Load array texture from multiple image files
    // All images must have the same dimensions and channel count
    TextureHandle LoadArray(
        const std::vector<std::string>& layerPaths,
        const TextureLoadOptions& options);

    TextureHandle LoadArray(const std::vector<std::string>& layerPaths) {
        return LoadArray(layerPaths, m_DefaultOptions);
    }

    // Load array texture using a pattern string (e.g., "textures/layer_{}.png")
    // Pattern must contain "{}" which will be replaced with layer indices [0, layerCount)
    TextureHandle LoadArrayPattern(
        const std::string& pathPattern,
        u32 layerCount,
        const TextureLoadOptions& options);

    TextureHandle LoadArrayPattern(const std::string& pathPattern, u32 layerCount) {
        return LoadArrayPattern(pathPattern, layerCount, m_DefaultOptions);
    }

    // ========================================================================
    // Cubemap Loading
    // ========================================================================

    // Load cubemap from 6 image files (faces: +X, -X, +Y, -Y, +Z, -Z)
    TextureHandle LoadCubemap(
        const std::vector<std::string>& facePaths,
        const TextureLoadOptions& options);

    TextureHandle LoadCubemap(const std::vector<std::string>& facePaths) {
        return LoadCubemap(facePaths, m_DefaultOptions);
    }

    // Load cubemap using a pattern string (e.g., "skybox/sky_{}.png")
    // Pattern must contain "{}" which will be replaced with face names
    TextureHandle LoadCubemapPattern(
        const std::string& pathPattern,
        const TextureLoadOptions& options);

    TextureHandle LoadCubemapPattern(const std::string& pathPattern) {
        return LoadCubemapPattern(pathPattern, m_DefaultOptions);
    }

    // Create procedural textures
    TextureHandle CreateSolid(u32 width, u32 height, const Vec4& color, TextureUsage usage = TextureUsage::Generic);
    TextureHandle CreateWhite();       // 1x1 white (for default albedo)
    TextureHandle CreateBlack();       // 1x1 black
    TextureHandle CreateNormalMap();   // 1x1 (0.5, 0.5, 1.0) - neutral normal in tangent space
    TextureHandle CreateMetalRough();  // 1x1 (rough=1.0, metal=0.5) - default PBR packed texture
    TextureHandle CreateCheckerboard(u32 size, const Vec4& color1, const Vec4& color2);  // Checkerboard pattern

    // ========================================================================
    // Asynchronous Loading API
    // ========================================================================

    // Callback signature: (TextureHandle handle, bool success, void* userData)
    using AsyncLoadCallback = void(*)(TextureHandle, bool, void*);

    // Load texture asynchronously using JobSystem
    // - Returns handle immediately (valid, but points to placeholder texture)
    // - File I/O happens on worker thread
    // - GPU upload happens on main thread during Update()
    // - Callback fires after GPU upload completes (or on failure)
    TextureHandle LoadAsync(
        const std::string& filepath,
        const TextureLoadOptions& options,
        AsyncLoadCallback callback,
        void* userData = nullptr
    );

    // Overload with default options
    TextureHandle LoadAsync(
        const std::string& filepath,
        AsyncLoadCallback callback,
        void* userData = nullptr
    ) {
        return LoadAsync(filepath, m_DefaultOptions, callback, userData);
    }

    // Load array texture asynchronously
    TextureHandle LoadArrayAsync(
        const std::vector<std::string>& layerPaths,
        const TextureLoadOptions& options,
        AsyncLoadCallback callback,
        void* userData = nullptr
    );

    TextureHandle LoadArrayAsync(
        const std::vector<std::string>& layerPaths,
        AsyncLoadCallback callback,
        void* userData = nullptr
    ) {
        return LoadArrayAsync(layerPaths, m_DefaultOptions, callback, userData);
    }

    // Process pending GPU uploads (call once per frame from main thread)
    // This checks for completed async uploads and processes pending CPU uploads
    void Update();

    // Process pending GPU uploads with bandwidth limiting
    // maxBytesPerFrame: Maximum number of bytes to upload this frame (default 16MB)
    void ProcessUploads(u64 maxBytesPerFrame = 16 * 1024 * 1024);

    // Enqueue a completed load job for GPU upload (called by worker threads)
    void EnqueuePendingUpload(TextureLoadJob* job);

    // ========================================================================
    // Hot Reload API
    // ========================================================================

    // Enable/disable hot reload (starts/stops file watching)
    void EnableHotReload(bool enable);

    // Add directory to watch for texture changes
    void WatchDirectory(const std::string& directory, bool recursive = true);

    // Reload texture from disk (by handle or filepath)
    // Returns true if reload was initiated, false on error
    bool ReloadTexture(TextureHandle handle);
    bool ReloadTexture(const std::string& filepath);

    // Get placeholder and error textures
    TextureHandle GetPlaceholderTexture() const { return m_PlaceholderTexture; }
    TextureHandle GetErrorTexture() const { return m_ErrorTexture; }

    // Get descriptor index for a texture (for bindless array access)
    u32 GetDescriptorIndex(TextureHandle handle) const;

protected:
    // Override ResourceManager::LoadResource to use default options
    std::unique_ptr<TextureData> LoadResource(const std::string& filepath) override;

private:
    TextureManager();
    ~TextureManager() = default;

    // Prevent copying
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    // Helper to create a single-pixel texture
    TextureHandle CreateSinglePixel(u8 r, u8 g, u8 b, u8 a, TextureUsage usage);

    // Helper methods for async loading
    void ProcessUpload(TextureLoadJob* job);
    void ProcessFailure(TextureLoadJob* job);

    // Default load options
    TextureLoadOptions m_DefaultOptions;

    // Cache for default textures
    TextureHandle m_WhiteTexture;
    TextureHandle m_BlackTexture;
    TextureHandle m_NormalMapTexture;
    TextureHandle m_MetalRoughTexture;
    TextureHandle m_PlaceholderTexture;  // Gray/white checkerboard (for hot reload)
    TextureHandle m_ErrorTexture;        // Magenta/black checkerboard (for failed reloads)

    // ========================================================================
    // Async Loading State
    // ========================================================================
    Platform::MutexPtr m_AsyncMutex;
    std::vector<TextureLoadJob*> m_PendingUploads;  // Jobs ready for GPU upload
    PoolAllocator<TextureLoadJob, 32> m_JobAllocator;

    // ========================================================================
    // Async GPU Upload Pipeline
    // ========================================================================
    VulkanContext* m_VulkanContext = nullptr;
    VulkanTransferQueue* m_TransferQueue = nullptr;
    VulkanStagingPool* m_StagingPool = nullptr;

    // Track pending GPU uploads with timeline values
    struct PendingGPUUpload {
        TextureHandle handle;
        u64 timelineValue;
        AsyncLoadCallback callback;
        void* userData;
        u64 uploadSizeBytes;
    };
    std::vector<PendingGPUUpload> m_GPUUploadQueue;
    Platform::MutexPtr m_GPUUploadMutex;

    // ========================================================================
    // Hot Reload State
    // ========================================================================
    FileWatcher m_FileWatcher;
    bool m_HotReloadEnabled = false;
    std::unordered_set<std::string> m_WatchedDirectories;

    // Hot reload helpers
    void OnTextureFileModified(const std::string& filepath, FileAction action);
    void PerformReload(TextureHandle handle, const std::string& filepath);
};
