#include "renderer/vulkan_context.h"
#include "renderer/vulkan_descriptors.h"
#include "renderer/vulkan_texture.h"
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

// Helper to create synthetic texture
TextureData CreateTestTexture(u32 width, u32 height, u32 channels, TextureType type = TextureType::Texture2D) {
    TextureData data;
    data.width = width;
    data.height = height;
    data.channels = channels;
    data.arrayLayers = 1;
    data.type = type;
    data.mipLevels = 1;

    const u64 size = static_cast<u64>(width) * height * channels;
    data.pixels = static_cast<u8*>(malloc(size));
    memset(data.pixels, 128, size);

    return data;
}

// Helper to create array texture
TextureData CreateTestArrayTexture(u32 width, u32 height, u32 channels, u32 layers) {
    TextureData data;
    data.width = width;
    data.height = height;
    data.channels = channels;
    data.arrayLayers = layers;
    data.type = TextureType::TextureArray;
    data.mipLevels = 1;

    const u64 layerSize = static_cast<u64>(width) * height * channels;

    for (u32 i = 0; i < layers; ++i) {
        u8* layerData = static_cast<u8*>(malloc(layerSize));
        memset(layerData, (i + 1) * 40, layerSize);
        data.layerPixels.push_back(layerData);
    }

    if (!data.PackLayersIntoStagingBuffer()) {
        throw std::runtime_error("Failed to pack array texture layers");
    }

    return data;
}

TEST(Descriptors_InitWithTextureSampler) {
    WindowProperties props;
    props.title = "Descriptor Texture Test";
    props.width = 640;
    props.height = 480;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    VulkanDescriptors descriptors;
    descriptors.Init(&context, 2);  // 2 frames in flight

    // Verify layout and descriptor sets were created
    ASSERT(descriptors.GetLayout() != VK_NULL_HANDLE);
    ASSERT(descriptors.GetDescriptorSet(0) != VK_NULL_HANDLE);
    ASSERT(descriptors.GetDescriptorSet(1) != VK_NULL_HANDLE);

    descriptors.Shutdown();
    context.Shutdown();
}

TEST(Descriptors_BindRegularTexture) {
    WindowProperties props;
    props.title = "Bind Regular Texture Test";
    props.width = 640;
    props.height = 480;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    VulkanDescriptors descriptors;
    descriptors.Init(&context, 2);

    // Create a simple texture
    TextureData data = CreateTestTexture(64, 64, 4);
    VulkanTexture texture;
    texture.Create(&context, &data);

    ASSERT(texture.IsValid());

    // Bind texture to descriptor set (binding 1, frame 0)
    descriptors.BindTexture(0, 1, texture.GetImageView(), texture.GetSampler());

    // Should not throw - binding successful
    ASSERT(descriptors.GetDescriptorSet(0) != VK_NULL_HANDLE);

    texture.Destroy();
    descriptors.Shutdown();
    context.Shutdown();
}

TEST(Descriptors_BindArrayTexture) {
    WindowProperties props;
    props.title = "Bind Array Texture Test";
    props.width = 640;
    props.height = 480;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    VulkanDescriptors descriptors;
    descriptors.Init(&context, 2);

    // Create array texture with 4 layers
    TextureData data = CreateTestArrayTexture(64, 64, 4, 4);
    VulkanTexture texture;
    texture.Create(&context, &data);

    ASSERT(texture.IsValid());

    // Bind array texture to descriptor set
    descriptors.BindTextureArray(0, 1, texture.GetImageView(), texture.GetSampler());

    // Should work identically to regular BindTexture
    ASSERT(descriptors.GetDescriptorSet(0) != VK_NULL_HANDLE);

    texture.Destroy();
    descriptors.Shutdown();
    context.Shutdown();
}

TEST(Descriptors_BindMultipleFrames) {
    WindowProperties props;
    props.title = "Bind Multiple Frames Test";
    props.width = 640;
    props.height = 480;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    VulkanDescriptors descriptors;
    descriptors.Init(&context, 3);  // 3 frames

    // Create different textures for different frames
    TextureData data1 = CreateTestTexture(64, 64, 4);
    TextureData data2 = CreateTestTexture(128, 128, 4);
    TextureData data3 = CreateTestTexture(32, 32, 4);

    VulkanTexture texture1, texture2, texture3;
    texture1.Create(&context, &data1);
    texture2.Create(&context, &data2);
    texture3.Create(&context, &data3);

    // Bind different textures to different frames
    descriptors.BindTexture(0, 1, texture1.GetImageView(), texture1.GetSampler());
    descriptors.BindTexture(1, 1, texture2.GetImageView(), texture2.GetSampler());
    descriptors.BindTexture(2, 1, texture3.GetImageView(), texture3.GetSampler());

    // All descriptor sets should be valid
    ASSERT(descriptors.GetDescriptorSet(0) != VK_NULL_HANDLE);
    ASSERT(descriptors.GetDescriptorSet(1) != VK_NULL_HANDLE);
    ASSERT(descriptors.GetDescriptorSet(2) != VK_NULL_HANDLE);

    texture1.Destroy();
    texture2.Destroy();
    texture3.Destroy();
    descriptors.Shutdown();
    context.Shutdown();
}

TEST(Descriptors_RebindTexture) {
    WindowProperties props;
    props.title = "Rebind Texture Test";
    props.width = 640;
    props.height = 480;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    VulkanDescriptors descriptors;
    descriptors.Init(&context, 2);

    // Create two textures
    TextureData data1 = CreateTestTexture(64, 64, 4);
    TextureData data2 = CreateTestTexture(128, 128, 4);

    VulkanTexture texture1, texture2;
    texture1.Create(&context, &data1);
    texture2.Create(&context, &data2);

    // Bind first texture
    descriptors.BindTexture(0, 1, texture1.GetImageView(), texture1.GetSampler());

    // Rebind to different texture (should update descriptor)
    descriptors.BindTexture(0, 1, texture2.GetImageView(), texture2.GetSampler());

    // Should succeed without errors
    ASSERT(descriptors.GetDescriptorSet(0) != VK_NULL_HANDLE);

    texture1.Destroy();
    texture2.Destroy();
    descriptors.Shutdown();
    context.Shutdown();
}

TEST(Descriptors_BindInvalidFrameIndex) {
    WindowProperties props;
    props.title = "Bind Invalid Frame Index Test";
    props.width = 640;
    props.height = 480;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    VulkanDescriptors descriptors;
    descriptors.Init(&context, 2);

    TextureData data = CreateTestTexture(64, 64, 4);
    VulkanTexture texture;
    texture.Create(&context, &data);

    // Try to bind to invalid frame index (should throw)
    bool threw = false;
    try {
        descriptors.BindTexture(5, 1, texture.GetImageView(), texture.GetSampler());
    } catch (const std::out_of_range&) {
        threw = true;
    }

    ASSERT(threw);

    texture.Destroy();
    descriptors.Shutdown();
    context.Shutdown();
}

TEST(Descriptors_BindNullHandles) {
    WindowProperties props;
    props.title = "Bind Null Handles Test";
    props.width = 640;
    props.height = 480;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    VulkanDescriptors descriptors;
    descriptors.Init(&context, 2);

    // Try to bind null image view (should throw)
    bool threw = false;
    try {
        descriptors.BindTexture(0, 1, VK_NULL_HANDLE, VK_NULL_HANDLE);
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    ASSERT(threw);

    descriptors.Shutdown();
    context.Shutdown();
}

TEST(Descriptors_BindMixedTextureTypes) {
    WindowProperties props;
    props.title = "Bind Mixed Texture Types Test";
    props.width = 640;
    props.height = 480;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    VulkanDescriptors descriptors;
    descriptors.Init(&context, 2);

    // Create regular texture for frame 0
    TextureData data1 = CreateTestTexture(64, 64, 4);
    VulkanTexture texture1;
    texture1.Create(&context, &data1);

    // Create array texture for frame 1
    TextureData data2 = CreateTestArrayTexture(64, 64, 4, 3);
    VulkanTexture texture2;
    texture2.Create(&context, &data2);

    // Bind different texture types to different frames
    descriptors.BindTexture(0, 1, texture1.GetImageView(), texture1.GetSampler());
    descriptors.BindTextureArray(1, 1, texture2.GetImageView(), texture2.GetSampler());

    // Both should work
    ASSERT(descriptors.GetDescriptorSet(0) != VK_NULL_HANDLE);
    ASSERT(descriptors.GetDescriptorSet(1) != VK_NULL_HANDLE);

    texture1.Destroy();
    texture2.Destroy();
    descriptors.Shutdown();
    context.Shutdown();
}

int main() {
    std::cout << "=== Texture Descriptor Binding Tests ===" << std::endl << std::endl;

    Descriptors_InitWithTextureSampler_runner();
    Descriptors_BindRegularTexture_runner();
    Descriptors_BindArrayTexture_runner();
    Descriptors_BindMultipleFrames_runner();
    Descriptors_RebindTexture_runner();
    Descriptors_BindInvalidFrameIndex_runner();
    Descriptors_BindNullHandles_runner();
    Descriptors_BindMixedTextureTypes_runner();

    std::cout << std::endl;
    std::cout << "Tests run:    " << testsRun << std::endl;
    std::cout << "Tests passed: " << testsPassed << std::endl;
    std::cout << "Tests failed: " << testsFailed << std::endl;

    return (testsFailed == 0) ? 0 : 1;
}
