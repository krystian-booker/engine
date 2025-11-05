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
    ASSERT(descriptors.GetPersistentLayout() != VK_NULL_HANDLE);
    ASSERT(descriptors.GetPersistentSet() != VK_NULL_HANDLE);

    descriptors.Shutdown();
    context.Shutdown();
}

TEST(Descriptors_RegisterRegularTexture) {
    WindowProperties props;
    props.title = "Register Regular Texture Test";
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

    // Register texture in bindless array
    u32 textureIndex = descriptors.RegisterTexture(texture.GetImageView(), texture.GetSampler());

    // Should return a valid index
    ASSERT(textureIndex < VulkanDescriptors::MAX_BINDLESS_TEXTURES);

    texture.Destroy();
    descriptors.Shutdown();
    context.Shutdown();
}

TEST(Descriptors_RegisterArrayTexture) {
    WindowProperties props;
    props.title = "Register Array Texture Test";
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

    // Register array texture (works identically to regular textures)
    u32 textureIndex = descriptors.RegisterTexture(texture.GetImageView(), texture.GetSampler());

    // Should return a valid index
    ASSERT(textureIndex < VulkanDescriptors::MAX_BINDLESS_TEXTURES);

    texture.Destroy();
    descriptors.Shutdown();
    context.Shutdown();
}

TEST(Descriptors_RegisterMultipleTextures) {
    WindowProperties props;
    props.title = "Register Multiple Textures Test";
    props.width = 640;
    props.height = 480;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    VulkanDescriptors descriptors;
    descriptors.Init(&context, 2);

    // Create different textures
    TextureData data1 = CreateTestTexture(64, 64, 4);
    TextureData data2 = CreateTestTexture(128, 128, 4);
    TextureData data3 = CreateTestTexture(32, 32, 4);

    VulkanTexture texture1, texture2, texture3;
    texture1.Create(&context, &data1);
    texture2.Create(&context, &data2);
    texture3.Create(&context, &data3);

    // Register all textures
    u32 index1 = descriptors.RegisterTexture(texture1.GetImageView(), texture1.GetSampler());
    u32 index2 = descriptors.RegisterTexture(texture2.GetImageView(), texture2.GetSampler());
    u32 index3 = descriptors.RegisterTexture(texture3.GetImageView(), texture3.GetSampler());

    // All indices should be unique and valid
    ASSERT(index1 < VulkanDescriptors::MAX_BINDLESS_TEXTURES);
    ASSERT(index2 < VulkanDescriptors::MAX_BINDLESS_TEXTURES);
    ASSERT(index3 < VulkanDescriptors::MAX_BINDLESS_TEXTURES);
    ASSERT(index1 != index2);
    ASSERT(index2 != index3);
    ASSERT(index1 != index3);

    texture1.Destroy();
    texture2.Destroy();
    texture3.Destroy();
    descriptors.Shutdown();
    context.Shutdown();
}

TEST(Descriptors_UnregisterTexture) {
    WindowProperties props;
    props.title = "Unregister Texture Test";
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

    // Register first texture
    u32 index1 = descriptors.RegisterTexture(texture1.GetImageView(), texture1.GetSampler());

    // Unregister it
    descriptors.UnregisterTexture(index1);

    // Register second texture (should reuse the freed index)
    u32 index2 = descriptors.RegisterTexture(texture2.GetImageView(), texture2.GetSampler());

    // Should have reused the index
    ASSERT(index2 == index1);

    texture1.Destroy();
    texture2.Destroy();
    descriptors.Shutdown();
    context.Shutdown();
}

TEST(Descriptors_RegisterSequentialIndices) {
    WindowProperties props;
    props.title = "Register Sequential Indices Test";
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

    // Register multiple times and verify sequential indices
    u32 index1 = descriptors.RegisterTexture(texture.GetImageView(), texture.GetSampler());
    u32 index2 = descriptors.RegisterTexture(texture.GetImageView(), texture.GetSampler());
    u32 index3 = descriptors.RegisterTexture(texture.GetImageView(), texture.GetSampler());

    ASSERT(index1 == 0);
    ASSERT(index2 == 1);
    ASSERT(index3 == 2);

    texture.Destroy();
    descriptors.Shutdown();
    context.Shutdown();
}

TEST(Descriptors_MixedTextureTypes) {
    WindowProperties props;
    props.title = "Mixed Texture Types Test";
    props.width = 640;
    props.height = 480;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    VulkanDescriptors descriptors;
    descriptors.Init(&context, 2);

    // Create regular texture
    TextureData data1 = CreateTestTexture(64, 64, 4);
    VulkanTexture texture1;
    texture1.Create(&context, &data1);

    // Create array texture
    TextureData data2 = CreateTestArrayTexture(64, 64, 4, 3);
    VulkanTexture texture2;
    texture2.Create(&context, &data2);

    // Register both texture types
    u32 index1 = descriptors.RegisterTexture(texture1.GetImageView(), texture1.GetSampler());
    u32 index2 = descriptors.RegisterTexture(texture2.GetImageView(), texture2.GetSampler());

    // Both should work and have different indices
    ASSERT(index1 < VulkanDescriptors::MAX_BINDLESS_TEXTURES);
    ASSERT(index2 < VulkanDescriptors::MAX_BINDLESS_TEXTURES);
    ASSERT(index1 != index2);

    texture1.Destroy();
    texture2.Destroy();
    descriptors.Shutdown();
    context.Shutdown();
}

int main() {
    std::cout << "=== Texture Descriptor Binding Tests ===" << std::endl << std::endl;

    Descriptors_InitWithTextureSampler_runner();
    Descriptors_RegisterRegularTexture_runner();
    Descriptors_RegisterArrayTexture_runner();
    Descriptors_RegisterMultipleTextures_runner();
    Descriptors_UnregisterTexture_runner();
    Descriptors_RegisterSequentialIndices_runner();
    Descriptors_MixedTextureTypes_runner();

    std::cout << std::endl;
    std::cout << "Tests run:    " << testsRun << std::endl;
    std::cout << "Tests passed: " << testsPassed << std::endl;
    std::cout << "Tests failed: " << testsFailed << std::endl;

    return (testsFailed == 0) ? 0 : 1;
}
