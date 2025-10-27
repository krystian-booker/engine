#include "resources/image_loader.h"
#include <iostream>
#include <cstring>

// Define STB_IMAGE_IMPLEMENTATION in exactly one .cpp file
#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG  // Get human-readable error messages

// Disable warnings for stb_image (external code)
#ifdef _MSC_VER
    #pragma warning(push, 0)
#elif defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wall"
    #pragma GCC diagnostic ignored "-Wextra"
    #pragma GCC diagnostic ignored "-Wpedantic"
    #pragma GCC diagnostic ignored "-Wunused-but-set-variable"
    #pragma GCC diagnostic ignored "-Wunused-variable"
    #pragma GCC diagnostic ignored "-Wunused-function"
#endif

#include <stb_image.h>

#ifdef _MSC_VER
    #pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic pop
#endif

namespace ImageLoader {

ImageData LoadImage(const std::string& filepath, const TextureLoadOptions& options) {
    ImageData result;

    // Configure stb_image flip
    stbi_set_flip_vertically_on_load(options.flipVertical);

    // Load image data
    i32 width, height, channels;
    i32 desiredChannels = static_cast<i32>(options.desiredChannels);

    u8* pixels = stbi_load(filepath.c_str(), &width, &height, &channels, desiredChannels);

    if (!pixels) {
        std::cerr << "ImageLoader::LoadImage failed to load \"" << filepath << "\": "
                  << stbi_failure_reason() << std::endl;
        return result;
    }

    // If desiredChannels was specified, stb_image converted the format
    if (desiredChannels > 0) {
        channels = desiredChannels;
    }

    // Validate channel count
    if (channels < 1 || channels > 4) {
        std::cerr << "ImageLoader::LoadImage unsupported channel count (" << channels
                  << ") for \"" << filepath << "\"" << std::endl;
        stbi_image_free(pixels);
        return result;
    }

    // Populate result
    result.pixels = pixels;
    result.width = static_cast<u32>(width);
    result.height = static_cast<u32>(height);
    result.channels = static_cast<u32>(channels);

    return result;
}

void FreeImage(ImageData& data) {
    if (data.pixels) {
        stbi_image_free(data.pixels);
        data.pixels = nullptr;
    }
    data.width = 0;
    data.height = 0;
    data.channels = 0;
}

bool GetImageInfo(const std::string& filepath, u32* width, u32* height, u32* channels) {
    i32 w, h, c;

    // stbi_info queries image metadata without loading pixel data
    i32 result = stbi_info(filepath.c_str(), &w, &h, &c);

    if (!result) {
        std::cerr << "ImageLoader::GetImageInfo failed for \"" << filepath << "\": "
                  << stbi_failure_reason() << std::endl;
        return false;
    }

    if (width) *width = static_cast<u32>(w);
    if (height) *height = static_cast<u32>(h);
    if (channels) *channels = static_cast<u32>(c);

    return true;
}

} // namespace ImageLoader
