#include "renderer/vulkan_context.h"
#include "renderer/vulkan_texture.h"
#include "renderer/mipmap_policy.h"
#include "core/texture_data.h"
#include "platform/window.h"
#include <iostream>
#include <stdexcept>
#include <cstring>

static int testsRun = 0;
static int testsPassed = 0;
static int testsFailed = 0;

#define TEST(name) \
    static void name(); \
    static void name##_runner() { \
        testsRun++; \
        std::cout << "Running " << #name << "... "; \
        try { \
            name(); \
            testsPassed++; \
            std::cout << "PASSED" << std::endl; \
        } catch (const std::exception& ex) { \
            testsFailed++; \
            std::cout << "FAILED (" << ex.what() << ")" << std::endl; \
        } catch (...) { \
            testsFailed++; \
            std::cout << "FAILED (unknown exception)" << std::endl; \
        } \
    } \
    static void name()

#define ASSERT(expr) \
    do { \
        if (!(expr)) { \
            throw std::runtime_error("Assertion failed: " #expr); \
        } \
    } while (0)

// Helper to create synthetic array texture data
TextureData CreateTestArrayTexture(u32 width, u32 height, u32 channels, u32 layers) {
    TextureData data;
    data.width = width;
    data.height = height;
    data.channels = channels;
    data.arrayLayers = layers;
    data.type = TextureType::TextureArray;
    data.mipLevels = 1;  // No mipmaps for basic test

    const u64 layerSize = static_cast<u64>(width) * height * channels;

    // Create distinct patterns for each layer
    for (u32 i = 0; i < layers; ++i) {
        u8* layerData = static_cast<u8*>(malloc(layerSize));
        memset(layerData, (i + 1) * 30, layerSize);  // Different intensity per layer
        data.layerPixels.push_back(layerData);
    }

    // Pack into staging buffer
    if (!data.PackLayersIntoStagingBuffer()) {
        throw std::runtime_error("Failed to pack array texture layers");
    }

    return data;
}

TEST(VulkanTexture_ArrayCreation) {
    WindowProperties props;
    props.title = "Array Texture Test";
    props.width = 640;
    props.height = 480;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    // Create 2-layer array texture
    TextureData data = CreateTestArrayTexture(64, 64, 4, 2);
    ASSERT(data.arrayLayers == 2);
    ASSERT(data.type == TextureType::TextureArray);

    VulkanTexture texture;
    texture.Create(&context, &data);

    ASSERT(texture.IsValid());
    ASSERT(texture.GetImage() != VK_NULL_HANDLE);
    ASSERT(texture.GetImageView() != VK_NULL_HANDLE);
    ASSERT(texture.GetSampler() != VK_NULL_HANDLE);
    ASSERT(texture.GetMipLevels() == 1);

    texture.Destroy();
    context.Shutdown();
}

TEST(VulkanTexture_ArrayMultipleLayers) {
    WindowProperties props;
    props.title = "Array Texture Multiple Layers Test";
    props.width = 640;
    props.height = 480;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    // Create 4-layer array texture
    TextureData data = CreateTestArrayTexture(128, 128, 4, 4);
    ASSERT(data.arrayLayers == 4);

    VulkanTexture texture;
    texture.Create(&context, &data);

    ASSERT(texture.IsValid());
    ASSERT(texture.GetImage() != VK_NULL_HANDLE);

    texture.Destroy();
    context.Shutdown();
}

TEST(VulkanTexture_ArrayWithMipmaps) {
    WindowProperties props;
    props.title = "Array Texture Mipmaps Test";
    props.width = 640;
    props.height = 480;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    // Create array texture with mipmap generation
    TextureData data = CreateTestArrayTexture(256, 256, 4, 2);
    data.mipLevels = 5;  // 256 -> 128 -> 64 -> 32 -> 16
    data.mipmapPolicy = MipmapPolicy::Auto;

    VulkanTexture texture;
    texture.Create(&context, &data);

    ASSERT(texture.IsValid());
    ASSERT(texture.GetMipLevels() == 5);

    texture.Destroy();
    context.Shutdown();
}

TEST(VulkanTexture_ArrayDifferentFormats) {
    WindowProperties props;
    props.title = "Array Texture Formats Test";
    props.width = 640;
    props.height = 480;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    // Test RGBA format
    {
        TextureData data = CreateTestArrayTexture(64, 64, 4, 2);
        VulkanTexture texture;
        texture.Create(&context, &data);
        ASSERT(texture.IsValid());
        texture.Destroy();
    }

    // Test RGB format
    {
        TextureData data = CreateTestArrayTexture(64, 64, 3, 2);
        VulkanTexture texture;
        texture.Create(&context, &data);
        ASSERT(texture.IsValid());
        texture.Destroy();
    }

    // Test single-channel format
    {
        TextureData data = CreateTestArrayTexture(64, 64, 1, 2);
        VulkanTexture texture;
        texture.Create(&context, &data);
        ASSERT(texture.IsValid());
        texture.Destroy();
    }

    context.Shutdown();
}

TEST(VulkanTexture_SingleLayerVsArray) {
    WindowProperties props;
    props.title = "Single vs Array Texture Test";
    props.width = 640;
    props.height = 480;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    // Single-layer texture (not an array)
    {
        TextureData data;
        data.width = 64;
        data.height = 64;
        data.channels = 4;
        data.arrayLayers = 1;
        data.type = TextureType::Texture2D;
        data.mipLevels = 1;

        const u64 size = 64 * 64 * 4;
        data.pixels = static_cast<u8*>(malloc(size));
        memset(data.pixels, 128, size);

        VulkanTexture texture;
        texture.Create(&context, &data);
        ASSERT(texture.IsValid());
        texture.Destroy();
    }

    // Array texture with 1 layer (should still work)
    {
        TextureData data = CreateTestArrayTexture(64, 64, 4, 1);
        VulkanTexture texture;
        texture.Create(&context, &data);
        ASSERT(texture.IsValid());
        texture.Destroy();
    }

    context.Shutdown();
}

TEST(VulkanTexture_ArrayLargeLayerCount) {
    WindowProperties props;
    props.title = "Array Texture Large Layer Count Test";
    props.width = 640;
    props.height = 480;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    // Create array with 16 layers (reasonable for terrain blending, etc.)
    TextureData data = CreateTestArrayTexture(64, 64, 4, 16);
    ASSERT(data.arrayLayers == 16);

    VulkanTexture texture;
    texture.Create(&context, &data);

    ASSERT(texture.IsValid());

    texture.Destroy();
    context.Shutdown();
}

int main() {
    std::cout << "=== VulkanTexture Array Tests ===" << std::endl << std::endl;

    VulkanTexture_ArrayCreation_runner();
    VulkanTexture_ArrayMultipleLayers_runner();
    VulkanTexture_ArrayWithMipmaps_runner();
    VulkanTexture_ArrayDifferentFormats_runner();
    VulkanTexture_SingleLayerVsArray_runner();
    VulkanTexture_ArrayLargeLayerCount_runner();

    std::cout << std::endl;
    std::cout << "Tests run:    " << testsRun << std::endl;
    std::cout << "Tests passed: " << testsPassed << std::endl;
    std::cout << "Tests failed: " << testsFailed << std::endl;

    return (testsFailed == 0) ? 0 : 1;
}
