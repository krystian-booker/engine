#pragma once
#include "core/types.h"
#include "core/texture_load_options.h"
#include <string>

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

    // Free image data allocated by LoadImage
    void FreeImage(ImageData& data);

    // Query image dimensions without loading full data
    bool GetImageInfo(const std::string& filepath, u32* width, u32* height, u32* channels);
}
