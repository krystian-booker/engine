#include "resources/image_loader.h"
#include <iostream>
#include <cstring>
#include <cstdlib>

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

ImageData LoadImageFromMemory(
    const u8* buffer,
    size_t bufferSize,
    const TextureLoadOptions& options)
{
    ImageData result;

    if (!buffer || bufferSize == 0) {
        std::cerr << "ImageLoader::LoadImageFromMemory: invalid buffer" << std::endl;
        return result;
    }

    // Configure stb_image flip
    stbi_set_flip_vertically_on_load(options.flipVertical);

    // Load image data from memory
    i32 width, height, channels;
    i32 desiredChannels = static_cast<i32>(options.desiredChannels);

    u8* pixels = stbi_load_from_memory(
        buffer,
        static_cast<i32>(bufferSize),
        &width,
        &height,
        &channels,
        desiredChannels);

    if (!pixels) {
        std::cerr << "ImageLoader::LoadImageFromMemory failed to decode image: "
                  << stbi_failure_reason() << std::endl;
        return result;
    }

    // If desiredChannels was specified, stb_image converted the format
    if (desiredChannels > 0) {
        channels = desiredChannels;
    }

    // Validate channel count
    if (channels < 1 || channels > 4) {
        std::cerr << "ImageLoader::LoadImageFromMemory unsupported channel count (" << channels << ")" << std::endl;
        stbi_image_free(pixels);
        return result;
    }

    // Convert RGB to RGBA (3-channel formats have poor GPU support)
    if (channels == 3) {
        const size_t pixelCount = static_cast<size_t>(width) * height;
        const size_t rgba_size = pixelCount * 4;
        u8* rgba_pixels = static_cast<u8*>(malloc(rgba_size));

        if (!rgba_pixels) {
            std::cerr << "ImageLoader: Failed to allocate memory for RGBA conversion" << std::endl;
            stbi_image_free(pixels);
            return result;
        }

        for (size_t i = 0; i < pixelCount; ++i) {
            rgba_pixels[i * 4 + 0] = pixels[i * 3 + 0];  // R
            rgba_pixels[i * 4 + 1] = pixels[i * 3 + 1];  // G
            rgba_pixels[i * 4 + 2] = pixels[i * 3 + 2];  // B
            rgba_pixels[i * 4 + 3] = 255;                // A (fully opaque)
        }

        stbi_image_free(pixels);
        pixels = rgba_pixels;
        channels = 4;

        std::cout << "ImageLoader: Converted RGB to RGBA for better GPU compatibility" << std::endl;
    }

    // Populate result
    result.pixels = pixels;
    result.width = static_cast<u32>(width);
    result.height = static_cast<u32>(height);
    result.channels = static_cast<u32>(channels);

    std::cout << "ImageLoader: Decoded image from memory - " << width << "x" << height
              << " (" << channels << " channels)" << std::endl;

    return result;
}

ImageData CreateImageFromRawData(
    const u8* pixelData,
    u32 width,
    u32 height,
    u32 channels,
    bool isBGRA)
{
    ImageData result;

    if (!pixelData || width == 0 || height == 0 || channels == 0) {
        std::cerr << "ImageLoader::CreateImageFromRawData: invalid parameters" << std::endl;
        return result;
    }

    if (channels < 1 || channels > 4) {
        std::cerr << "ImageLoader::CreateImageFromRawData: unsupported channel count (" << channels << ")" << std::endl;
        return result;
    }

    // Calculate buffer size
    size_t dataSize = static_cast<size_t>(width) * height * channels;

    // Allocate new buffer (use malloc to match stbi_image_free)
    u8* pixels = static_cast<u8*>(malloc(dataSize));
    if (!pixels) {
        std::cerr << "ImageLoader::CreateImageFromRawData: failed to allocate buffer" << std::endl;
        return result;
    }

    // Copy and optionally convert BGRA → RGBA
    if (isBGRA && channels >= 3) {
        for (size_t i = 0; i < width * height; ++i) {
            size_t srcIdx = i * channels;
            size_t dstIdx = i * channels;

            // BGRA → RGBA swap
            pixels[dstIdx + 0] = pixelData[srcIdx + 2];  // B → R
            pixels[dstIdx + 1] = pixelData[srcIdx + 1];  // G → G
            pixels[dstIdx + 2] = pixelData[srcIdx + 0];  // R → B

            if (channels == 4) {
                pixels[dstIdx + 3] = pixelData[srcIdx + 3];  // A → A
            }
        }
    } else {
        // Direct copy (no conversion needed)
        std::memcpy(pixels, pixelData, dataSize);
    }

    // Populate result
    result.pixels = pixels;
    result.width = width;
    result.height = height;
    result.channels = channels;

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

std::vector<ImageData> LoadImageArray(
    const std::vector<std::string>& filepaths,
    const TextureLoadOptions& options)
{
    std::vector<ImageData> layers;

    // Validate input
    if (filepaths.empty()) {
        std::cerr << "ImageLoader::LoadImageArray: empty filepath list" << std::endl;
        return layers;
    }

    // Load first layer to establish dimensions
    ImageData firstLayer = LoadImage(filepaths[0], options);
    if (!firstLayer.IsValid()) {
        std::cerr << "ImageLoader::LoadImageArray: failed to load first layer \""
                  << filepaths[0] << "\"" << std::endl;
        return layers;
    }

    layers.push_back(firstLayer);

    // Load remaining layers and validate dimensions match
    const u32 expectedWidth = firstLayer.width;
    const u32 expectedHeight = firstLayer.height;
    const u32 expectedChannels = firstLayer.channels;

    for (u32 i = 1; i < filepaths.size(); ++i) {
        ImageData layer = LoadImage(filepaths[i], options);

        if (!layer.IsValid()) {
            std::cerr << "ImageLoader::LoadImageArray: failed to load layer " << i
                      << " (\"" << filepaths[i] << "\")" << std::endl;
            // Clean up already-loaded layers
            for (ImageData& loadedLayer : layers) {
                FreeImage(loadedLayer);
            }
            return std::vector<ImageData>();  // Return empty vector
        }

        // Validate dimensions match first layer
        if (layer.width != expectedWidth || layer.height != expectedHeight || layer.channels != expectedChannels) {
            std::cerr << "ImageLoader::LoadImageArray: dimension mismatch at layer " << i << std::endl;
            std::cerr << "  Expected: " << expectedWidth << "x" << expectedHeight
                      << " (" << expectedChannels << " channels)" << std::endl;
            std::cerr << "  Got: " << layer.width << "x" << layer.height
                      << " (" << layer.channels << " channels) in \"" << filepaths[i] << "\"" << std::endl;

            // Clean up
            FreeImage(layer);
            for (ImageData& loadedLayer : layers) {
                FreeImage(loadedLayer);
            }
            return std::vector<ImageData>();  // Return empty vector
        }

        layers.push_back(layer);
    }

    return layers;
}

std::vector<ImageData> LoadImageArrayPattern(
    const std::string& filepathPattern,
    u32 layerCount,
    const TextureLoadOptions& options)
{
    // Validate pattern contains "{}"
    size_t placeholderPos = filepathPattern.find("{}");
    if (placeholderPos == std::string::npos) {
        std::cerr << "ImageLoader::LoadImageArrayPattern: pattern must contain \"{}\" placeholder" << std::endl;
        return std::vector<ImageData>();
    }

    // Generate filepaths from pattern
    std::vector<std::string> filepaths;
    filepaths.reserve(layerCount);

    for (u32 i = 0; i < layerCount; ++i) {
        std::string filepath = filepathPattern;
        std::string indexStr = std::to_string(i);

        // Replace "{}" with index
        filepath.replace(placeholderPos, 2, indexStr);
        filepaths.push_back(filepath);
    }

    // Use LoadImageArray to load all layers
    return LoadImageArray(filepaths, options);
}

std::vector<ImageData> LoadCubemap(
    const std::vector<std::string>& facePaths,
    const TextureLoadOptions& options)
{
    // Cubemap must have exactly 6 faces
    if (facePaths.size() != 6) {
        std::cerr << "ImageLoader::LoadCubemap: expected 6 face paths, got " << facePaths.size() << std::endl;
        return std::vector<ImageData>();
    }

    // Load all 6 faces using LoadImageArray
    std::vector<ImageData> faces = LoadImageArray(facePaths, options);

    if (faces.empty()) {
        return std::vector<ImageData>();  // LoadImageArray already printed error
    }

    // Additional validation: cubemap faces must be square
    if (faces[0].width != faces[0].height) {
        std::cerr << "ImageLoader::LoadCubemap: cubemap faces must be square" << std::endl;
        std::cerr << "  Got: " << faces[0].width << "x" << faces[0].height << std::endl;

        // Clean up
        for (ImageData& face : faces) {
            FreeImage(face);
        }
        return std::vector<ImageData>();
    }

    return faces;
}

std::vector<ImageData> LoadCubemapPattern(
    const std::string& filepathPattern,
    const TextureLoadOptions& options)
{
    // Validate pattern contains "{}"
    size_t placeholderPos = filepathPattern.find("{}");
    if (placeholderPos == std::string::npos) {
        std::cerr << "ImageLoader::LoadCubemapPattern: pattern must contain \"{}\" placeholder" << std::endl;
        return std::vector<ImageData>();
    }

    // Cubemap face names in order: +X, -X, +Y, -Y, +Z, -Z
    const char* faceNames[6] = {"px", "nx", "py", "ny", "pz", "nz"};

    // Generate filepaths from pattern
    std::vector<std::string> filepaths;
    filepaths.reserve(6);

    for (u32 i = 0; i < 6; ++i) {
        std::string filepath = filepathPattern;
        filepath.replace(placeholderPos, 2, faceNames[i]);
        filepaths.push_back(filepath);
    }

    // Use LoadCubemap to load and validate all faces
    return LoadCubemap(filepaths, options);
}

} // namespace ImageLoader
