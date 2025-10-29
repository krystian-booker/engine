#pragma once
#include "core/types.h"
#include "core/texture_load_options.h"
#include <string>
#include <vector>

// Loaded image data (CPU-side)
struct ImageData {
    u8* pixels = nullptr;
    u32 width = 0;
    u32 height = 0;
    u32 channels = 0;  // 1=R, 2=RG, 3=RGB, 4=RGBA

    bool IsValid() const {
        return pixels != nullptr && width > 0 && height > 0 && channels > 0;
    }
};

// Image loading utilities (wraps stb_image)
namespace ImageLoader {
    // Load image from file with options
    // Returns ImageData with allocated pixel buffer (must call FreeImage)
    ImageData LoadImage(const std::string& filepath, const TextureLoadOptions& options);

    // Load array texture from multiple image files
    // All images must have the same dimensions and channel count
    // Returns vector of ImageData (one per layer), empty vector on failure
    // Each ImageData must be freed with FreeImage
    std::vector<ImageData> LoadImageArray(
        const std::vector<std::string>& filepaths,
        const TextureLoadOptions& options);

    // Load array texture using a pattern string (e.g., "textures/layer_{}.png")
    // Pattern must contain "{}" which will be replaced with layer indices [0, layerCount)
    // All images must have the same dimensions and channel count
    // Returns vector of ImageData (one per layer), empty vector on failure
    // Each ImageData must be freed with FreeImage
    std::vector<ImageData> LoadImageArrayPattern(
        const std::string& filepathPattern,
        u32 layerCount,
        const TextureLoadOptions& options);

    // Load cubemap from 6 image files (faces: +X, -X, +Y, -Y, +Z, -Z)
    // All faces must be square and have the same dimensions
    // Returns vector of 6 ImageData (one per face), empty vector on failure
    // Each ImageData must be freed with FreeImage
    std::vector<ImageData> LoadCubemap(
        const std::vector<std::string>& facePaths,
        const TextureLoadOptions& options);

    // Load cubemap using a pattern string (e.g., "skybox/sky_{}.png")
    // Pattern must contain "{}" which will be replaced with face names:
    // "px" (+X), "nx" (-X), "py" (+Y), "ny" (-Y), "pz" (+Z), "nz" (-Z)
    // All faces must be square and have the same dimensions
    // Returns vector of 6 ImageData (one per face), empty vector on failure
    // Each ImageData must be freed with FreeImage
    std::vector<ImageData> LoadCubemapPattern(
        const std::string& filepathPattern,
        const TextureLoadOptions& options);

    // Load image from memory buffer (compressed format: PNG, JPG, etc.)
    // Buffer must contain valid compressed image data
    // Returns ImageData with allocated pixel buffer (must call FreeImage)
    ImageData LoadImageFromMemory(
        const u8* buffer,
        size_t bufferSize,
        const TextureLoadOptions& options);

    // Create image from raw pixel data (RGBA or BGRA format)
    // Copies the provided pixel data into a new buffer
    // Returns ImageData with allocated pixel buffer (must call FreeImage)
    // isBGRA: if true, converts BGRA to RGBA
    ImageData CreateImageFromRawData(
        const u8* pixelData,
        u32 width,
        u32 height,
        u32 channels,
        bool isBGRA = false);

    // Free image data allocated by LoadImage
    void FreeImage(ImageData& data);

    // Query image dimensions without loading full data
    bool GetImageInfo(const std::string& filepath, u32* width, u32* height, u32* channels);
}
