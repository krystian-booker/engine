#pragma once
#include "core/resource_manager.h"
#include "core/texture_data.h"
#include "core/texture_load_options.h"
#include "core/math.h"
#include "core/memory.h"
#include "resources/texture_load_job.h"
#include "platform/platform.h"
#include <vector>

// Global texture configuration
namespace TextureConfig {
    extern u32 g_DefaultAnisotropy;
    void SetDefaultAnisotropy(u32 level);
    u32 GetDefaultAnisotropy();
}

// Texture resource manager (singleton)
class TextureManager : public ResourceManager<TextureData, TextureHandle> {
public:
    // Singleton access
    static TextureManager& Instance() {
        static TextureManager instance;
        return instance;
    }

    // Load texture from file with options
    TextureHandle Load(const std::string& filepath, const TextureLoadOptions& options);

    // Load with default options (inferred from file extension or usage)
    TextureHandle Load(const std::string& filepath) {
        return ResourceManager::Load(filepath);
    }

    // Create procedural textures
    TextureHandle CreateSolid(u32 width, u32 height, const Vec4& color, TextureUsage usage = TextureUsage::Generic);
    TextureHandle CreateWhite();       // 1x1 white (for default albedo)
    TextureHandle CreateBlack();       // 1x1 black
    TextureHandle CreateNormalMap();   // 1x1 (0.5, 0.5, 1.0) - neutral normal in tangent space

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

    // Process pending GPU uploads (call once per frame from main thread)
    void Update();

    // Enqueue a completed load job for GPU upload (called by worker threads)
    void EnqueuePendingUpload(TextureLoadJob* job);

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

    // ========================================================================
    // Async Loading State
    // ========================================================================
    Platform::MutexPtr m_AsyncMutex;
    std::vector<TextureLoadJob*> m_PendingUploads;  // Jobs ready for GPU upload
    PoolAllocator<TextureLoadJob, 32> m_JobAllocator;
};
