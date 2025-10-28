#pragma once
#include "core/types.h"
#include "core/resource_handle.h"
#include "core/texture_load_options.h"
#include "resources/texture_load_state.h"
#include "resources/image_loader.h"
#include <atomic>
#include <string>
#include <vector>

// Forward declaration
struct Job;

// Texture loading job for async loading via JobSystem
struct TextureLoadJob {
    // ========================================================================
    // Input Parameters (immutable after creation)
    // ========================================================================
    std::string filepath;                      // For single texture loading
    std::vector<std::string> layerPaths;       // For array texture loading
    bool isArrayTexture = false;               // True if loading array texture
    TextureLoadOptions options;
    TextureHandle handle;
    void* userData;

    // ========================================================================
    // Callback
    // ========================================================================
    // Callback signature: (TextureHandle handle, bool success, void* userData)
    using CallbackFn = void(*)(TextureHandle, bool, void*);
    CallbackFn callback;

    // ========================================================================
    // State Tracking (atomic for thread safety)
    // ========================================================================
    std::atomic<AsyncLoadState> state;

    // ========================================================================
    // Output Data (populated by worker thread)
    // ========================================================================
    ImageData imageData;                       // CPU-side pixel data for single texture
    std::vector<ImageData> layerImageData;     // CPU-side pixel data for array textures
    std::string errorMessage;                  // Error details if state == Failed

    // ========================================================================
    // Job System Integration
    // ========================================================================
    Job* job;  // Associated JobSystem job

    // ========================================================================
    // Constructor
    // ========================================================================
    TextureLoadJob()
        : userData(nullptr)
        , callback(nullptr)
        , state(AsyncLoadState::Pending)
        , job(nullptr)
    {}
};
