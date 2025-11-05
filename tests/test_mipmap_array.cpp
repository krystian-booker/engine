#include "renderer/vulkan_context.h"
#include "renderer/vulkan_texture.h"
#include "renderer/mipmap_policy.h"
#include "core/texture_data.h"
#include "platform/window.h"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cmath>

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

// Helper to calculate expected mip levels
u32 CalculateMipLevels(u32 width, u32 height) {
    u32 maxDim = std::max(width, height);
    return static_cast<u32>(std::floor(std::log2(maxDim))) + 1;
}

// Helper to create synthetic array texture data
TextureData CreateTestArrayTexture(u32 width, u32 height, u32 channels, u32 layers, MipmapPolicy policy) {
    TextureData data;
    data.width = width;
    data.height = height;
    data.channels = channels;
    data.arrayLayers = layers;
    data.type = TextureType::TextureArray;
    data.mipmapPolicy = policy;

    if (policy == MipmapPolicy::Auto) {
        data.mipLevels = CalculateMipLevels(width, height);
    } else {
        data.mipLevels = 1;
    }

    const u64 layerSize = static_cast<u64>(width) * height * channels;

    // Create distinct patterns for each layer
    for (u32 i = 0; i < layers; ++i) {
        u8* layerData = static_cast<u8*>(malloc(layerSize));

        // Create checkerboard pattern with different colors per layer
        for (u32 y = 0; y < height; ++y) {
            for (u32 x = 0; x < width; ++x) {
                u32 idx = (y * width + x) * channels;
                bool checker = ((x / 16) + (y / 16)) % 2;
                u8 value = static_cast<u8>(checker ? (255 - i * 30) : (i * 30));

                for (u32 c = 0; c < channels; ++c) {
                    layerData[idx + c] = value;
                }
            }
        }

        data.layerPixels.push_back(layerData);
    }

    // Pack into staging buffer
    if (!data.PackLayersIntoStagingBuffer()) {
        throw std::runtime_error("Failed to pack array texture layers");
    }

    return data;
}

TEST(Mipmap_ArrayTexture_BlitGeneration) {
    WindowProperties props;
    props.title = "Array Mipmap Blit Test";
    props.width = 640;
    props.height = 480;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    // Create 256x256 array with 2 layers
    TextureData data = CreateTestArrayTexture(256, 256, 4, 2, MipmapPolicy::Auto);
    data.qualityHint = MipmapQuality::Fast;  // Prefer blit

    ASSERT(data.mipLevels == 9);  // 256->128->64->32->16->8->4->2->1
    ASSERT(data.arrayLayers == 2);

    VulkanTexture texture;
    texture.Create(&context, &data);

    ASSERT(texture.IsValid());
    ASSERT(texture.GetMipLevels() == 9);

    texture.Destroy();
    context.Shutdown();
}

TEST(Mipmap_ArrayTexture_ComputeGeneration) {
    WindowProperties props;
    props.title = "Array Mipmap Compute Test";
    props.width = 640;
    props.height = 480;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    // Create array with 4 layers
    TextureData data = CreateTestArrayTexture(128, 128, 4, 4, MipmapPolicy::Auto);
    data.qualityHint = MipmapQuality::Balanced;  // Prefer compute

    ASSERT(data.mipLevels == 8);  // 128->64->32->16->8->4->2->1
    ASSERT(data.arrayLayers == 4);

    VulkanTexture texture;
    texture.Create(&context, &data);

    ASSERT(texture.IsValid());
    ASSERT(texture.GetMipLevels() == 8);

    texture.Destroy();
    context.Shutdown();
}

TEST(Mipmap_ArrayTexture_CPUGeneration) {
    WindowProperties props;
    props.title = "Array Mipmap CPU Test";
    props.width = 640;
    props.height = 480;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    // Create array with 3 layers
    TextureData data = CreateTestArrayTexture(64, 64, 4, 3, MipmapPolicy::Auto);
    data.qualityHint = MipmapQuality::High;  // May use CPU or compute

    ASSERT(data.mipLevels == 7);  // 64->32->16->8->4->2->1
    ASSERT(data.arrayLayers == 3);

    VulkanTexture texture;
    texture.Create(&context, &data);

    ASSERT(texture.IsValid());
    ASSERT(texture.GetMipLevels() == 7);

    texture.Destroy();
    context.Shutdown();
}

TEST(Mipmap_ArrayTexture_NoGeneration) {
    WindowProperties props;
    props.title = "Array No Mipmap Test";
    props.width = 640;
    props.height = 480;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    // Create array without mipmap generation
    // Note: CreateTestArrayTexture sets mipLevels=1 for non-Auto policies
    TextureData data = CreateTestArrayTexture(128, 128, 4, 2, MipmapPolicy::ForceCPU);

    ASSERT(data.mipLevels == 1);
    ASSERT(data.arrayLayers == 2);

    VulkanTexture texture;
    texture.Create(&context, &data);

    ASSERT(texture.IsValid());
    ASSERT(texture.GetMipLevels() == 1);

    texture.Destroy();
    context.Shutdown();
}

TEST(Mipmap_ArrayTexture_NonPowerOfTwo) {
    WindowProperties props;
    props.title = "Array NPOT Mipmap Test";
    props.width = 640;
    props.height = 480;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    // Non-power-of-two dimensions
    TextureData data = CreateTestArrayTexture(100, 100, 4, 2, MipmapPolicy::Auto);

    // Mip levels based on max dimension (100 -> 50 -> 25 -> 12 -> 6 -> 3 -> 1)
    ASSERT(data.mipLevels == 7);
    ASSERT(data.arrayLayers == 2);

    VulkanTexture texture;
    texture.Create(&context, &data);

    ASSERT(texture.IsValid());
    ASSERT(texture.GetMipLevels() == 7);

    texture.Destroy();
    context.Shutdown();
}

TEST(Mipmap_ArrayTexture_RectangularDimensions) {
    WindowProperties props;
    props.title = "Array Rectangular Mipmap Test";
    props.width = 640;
    props.height = 480;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    // Rectangular dimensions (256x128)
    TextureData data = CreateTestArrayTexture(256, 128, 4, 2, MipmapPolicy::Auto);

    // Mip levels based on max dimension (256)
    ASSERT(data.mipLevels == 9);
    ASSERT(data.arrayLayers == 2);

    VulkanTexture texture;
    texture.Create(&context, &data);

    ASSERT(texture.IsValid());
    ASSERT(texture.GetMipLevels() == 9);

    texture.Destroy();
    context.Shutdown();
}

TEST(Mipmap_ArrayTexture_ManyLayers) {
    WindowProperties props;
    props.title = "Array Many Layers Mipmap Test";
    props.width = 640;
    props.height = 480;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    // Array with 8 layers and mipmaps
    TextureData data = CreateTestArrayTexture(128, 128, 4, 8, MipmapPolicy::Auto);
    data.qualityHint = MipmapQuality::Balanced;

    ASSERT(data.mipLevels == 8);
    ASSERT(data.arrayLayers == 8);

    VulkanTexture texture;
    texture.Create(&context, &data);

    ASSERT(texture.IsValid());
    ASSERT(texture.GetMipLevels() == 8);

    texture.Destroy();
    context.Shutdown();
}

TEST(Mipmap_ArrayTexture_SingleChannel) {
    WindowProperties props;
    props.title = "Array Single Channel Mipmap Test";
    props.width = 640;
    props.height = 480;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    // Single channel (grayscale) array with mipmaps
    TextureData data = CreateTestArrayTexture(64, 64, 1, 2, MipmapPolicy::Auto);

    ASSERT(data.mipLevels == 7);
    ASSERT(data.channels == 1);
    ASSERT(data.arrayLayers == 2);

    VulkanTexture texture;
    texture.Create(&context, &data);

    ASSERT(texture.IsValid());
    ASSERT(texture.GetMipLevels() == 7);

    texture.Destroy();
    context.Shutdown();
}

int main() {
    std::cout << "=== Array Texture Mipmap Tests ===" << std::endl << std::endl;

    Mipmap_ArrayTexture_BlitGeneration_runner();
    Mipmap_ArrayTexture_ComputeGeneration_runner();
    Mipmap_ArrayTexture_CPUGeneration_runner();
    Mipmap_ArrayTexture_NoGeneration_runner();
    Mipmap_ArrayTexture_NonPowerOfTwo_runner();
    Mipmap_ArrayTexture_RectangularDimensions_runner();
    Mipmap_ArrayTexture_ManyLayers_runner();
    Mipmap_ArrayTexture_SingleChannel_runner();

    std::cout << std::endl;
    std::cout << "Tests run:    " << testsRun << std::endl;
    std::cout << "Tests passed: " << testsPassed << std::endl;
    std::cout << "Tests failed: " << testsFailed << std::endl;

    return (testsFailed == 0) ? 0 : 1;
}
