#include "core/texture_data.h"
#include "resources/image_loader.h"
#include "resources/texture_manager.h"
#include "renderer/vulkan_context.h"
#include "renderer/vulkan_texture.h"
#include "renderer/mipmap_policy.h"
#include "platform/window.h"
#include <iostream>
#include <cassert>
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

// ============================================================================
// Test TextureData Cubemap Validation
// ============================================================================

TEST(TextureData_CubemapValidation_Valid) {
    TextureData data;
    data.type = TextureType::Cubemap;
    data.width = 512;
    data.height = 512;  // Square
    data.channels = 4;
    data.arrayLayers = 6;  // Exactly 6 faces

    assert(data.ValidateCubemap() == true);
}

TEST(TextureData_CubemapValidation_WrongLayerCount) {
    TextureData data;
    data.type = TextureType::Cubemap;
    data.width = 512;
    data.height = 512;
    data.channels = 4;
    data.arrayLayers = 4;  // Not 6 faces

    assert(data.ValidateCubemap() == false);
}

TEST(TextureData_CubemapValidation_NotSquare) {
    TextureData data;
    data.type = TextureType::Cubemap;
    data.width = 512;
    data.height = 256;  // Not square
    data.channels = 4;
    data.arrayLayers = 6;

    assert(data.ValidateCubemap() == false);
}

TEST(TextureData_CubemapValidation_WrongType) {
    TextureData data;
    data.type = TextureType::TextureArray;  // Not cubemap type
    data.width = 512;
    data.height = 512;
    data.channels = 4;
    data.arrayLayers = 6;

    assert(data.ValidateCubemap() == false);
}

// ============================================================================
// Test ImageLoader Cubemap Functions
// ============================================================================

TEST(ImageLoader_LoadCubemap_WrongCount) {
    std::vector<std::string> facePaths = {
        "face0.png",
        "face1.png",
        "face2.png",
        "face3.png"  // Only 4 faces, not 6
    };

    TextureLoadOptions options{};
    std::vector<ImageData> faces = ImageLoader::LoadCubemap(facePaths, options);

    assert(faces.empty());  // Should fail
}

TEST(ImageLoader_LoadCubemapPattern_InvalidPattern) {
    std::string pattern = "skybox/face.png";  // No {} placeholder
    TextureLoadOptions options{};

    std::vector<ImageData> faces = ImageLoader::LoadCubemapPattern(pattern, options);

    assert(faces.empty());  // Should fail
}

// ============================================================================
// Test VulkanTexture Cubemap Creation
// ============================================================================

// Helper to create synthetic cubemap data
TextureData CreateTestCubemap(u32 size, u32 channels) {
    TextureData data;
    data.width = size;
    data.height = size;  // Must be square
    data.channels = channels;
    data.arrayLayers = 6;  // Cubemap has 6 faces
    data.type = TextureType::Cubemap;
    data.mipLevels = 1;

    const u64 faceSize = static_cast<u64>(size) * size * channels;

    // Create 6 faces with different patterns
    for (u32 i = 0; i < 6; ++i) {
        u8* faceData = static_cast<u8*>(malloc(faceSize));
        memset(faceData, (i + 1) * 40, faceSize);
        data.layerPixels.push_back(faceData);
    }

    // Pack into staging buffer
    if (!data.PackLayersIntoStagingBuffer()) {
        throw std::runtime_error("Failed to pack cubemap faces");
    }

    return data;
}

TEST(VulkanTexture_CubemapCreation) {
    WindowProperties props;
    props.title = "Cubemap Test";
    props.width = 640;
    props.height = 480;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    // Create cubemap
    TextureData data = CreateTestCubemap(256, 4);
    assert(data.ValidateCubemap() == true);

    VulkanTexture texture;
    texture.Create(&context, &data);

    assert(texture.IsValid());
    assert(texture.GetImage() != VK_NULL_HANDLE);
    assert(texture.GetImageView() != VK_NULL_HANDLE);
    assert(texture.GetSampler() != VK_NULL_HANDLE);

    texture.Destroy();
    context.Shutdown();
}

TEST(VulkanTexture_CubemapWithMipmaps) {
    WindowProperties props;
    props.title = "Cubemap Mipmaps Test";
    props.width = 640;
    props.height = 480;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    // Create cubemap with mipmaps
    TextureData data = CreateTestCubemap(512, 4);
    data.mipLevels = 10;  // 512 -> 256 -> ... -> 1
    data.mipmapPolicy = MipmapPolicy::Auto;

    VulkanTexture texture;
    texture.Create(&context, &data);

    assert(texture.IsValid());
    assert(texture.GetMipLevels() == 10);

    texture.Destroy();
    context.Shutdown();
}

TEST(VulkanTexture_CubemapDifferentSizes) {
    WindowProperties props;
    props.title = "Cubemap Sizes Test";
    props.width = 640;
    props.height = 480;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    // Test 128x128
    {
        TextureData data = CreateTestCubemap(128, 4);
        VulkanTexture texture;
        texture.Create(&context, &data);
        assert(texture.IsValid());
        texture.Destroy();
    }

    // Test 1024x1024
    {
        TextureData data = CreateTestCubemap(1024, 4);
        VulkanTexture texture;
        texture.Create(&context, &data);
        assert(texture.IsValid());
        texture.Destroy();
    }

    context.Shutdown();
}

// ============================================================================
// Test TextureManager Cubemap Loading
// ============================================================================

TEST(TextureManager_LoadCubemap_WrongCount) {
    std::vector<std::string> facePaths = {
        "px.png", "nx.png", "py.png", "ny.png"  // Only 4 faces
    };

    TextureHandle handle = TextureManager::Instance().LoadCubemap(facePaths);
    assert(!handle.IsValid());  // Should fail
}

TEST(TextureManager_LoadCubemapPattern_InvalidPattern) {
    std::string pattern = "skybox/face.png";  // No {}

    TextureHandle handle = TextureManager::Instance().LoadCubemapPattern(pattern);
    assert(!handle.IsValid());  // Should fail
}

TEST(TextureManager_LoadCubemap_NonexistentFiles) {
    std::vector<std::string> facePaths = {
        "nonexistent_px.png",
        "nonexistent_nx.png",
        "nonexistent_py.png",
        "nonexistent_ny.png",
        "nonexistent_pz.png",
        "nonexistent_nz.png"
    };

    TextureHandle handle = TextureManager::Instance().LoadCubemap(facePaths);
    assert(!handle.IsValid());  // Should fail (files don't exist)
}

TEST(TextureManager_LoadCubemapPattern_NonexistentFiles) {
    std::string pattern = "nonexistent_skybox_{}.png";

    TextureHandle handle = TextureManager::Instance().LoadCubemapPattern(pattern);
    assert(!handle.IsValid());  // Should fail (files don't exist)
}

// Note: Full integration tests with actual cubemap images
// These tests verify the cubemap API behavior without requiring real assets
// Integration tests with real cubemap faces will be validated during build

int main() {
    std::cout << "=== Cubemap Tests ===" << std::endl << std::endl;

    // TextureData validation tests
    TextureData_CubemapValidation_Valid_runner();
    TextureData_CubemapValidation_WrongLayerCount_runner();
    TextureData_CubemapValidation_NotSquare_runner();
    TextureData_CubemapValidation_WrongType_runner();

    // ImageLoader tests
    ImageLoader_LoadCubemap_WrongCount_runner();
    ImageLoader_LoadCubemapPattern_InvalidPattern_runner();

    // VulkanTexture tests
    VulkanTexture_CubemapCreation_runner();
    VulkanTexture_CubemapWithMipmaps_runner();
    VulkanTexture_CubemapDifferentSizes_runner();

    // TextureManager tests
    TextureManager_LoadCubemap_WrongCount_runner();
    TextureManager_LoadCubemapPattern_InvalidPattern_runner();
    TextureManager_LoadCubemap_NonexistentFiles_runner();
    TextureManager_LoadCubemapPattern_NonexistentFiles_runner();

    std::cout << std::endl;
    std::cout << "Tests run:    " << testsRun << std::endl;
    std::cout << "Tests passed: " << testsPassed << std::endl;
    std::cout << "Tests failed: " << testsFailed << std::endl;

    std::cout << "\nNote: Full integration tests with actual cubemap images" << std::endl;
    std::cout << "will be validated during build with test assets." << std::endl;

    return (testsFailed == 0) ? 0 : 1;
}
