#include "resources/texture_manager.h"
#include "core/math.h"
#include <iostream>
#include <cassert>

#define TEST(name) \
    std::cout << "Running test: " << #name << "..." << std::endl; \
    name(); \
    std::cout << "  PASSED\n" << std::endl;

// Test global anisotropy configuration
void TestGlobalAnisotropyConfig() {
    // Test default value
    assert(TextureConfig::GetDefaultAnisotropy() == 16);

    // Test setter
    TextureConfig::SetDefaultAnisotropy(8);
    assert(TextureConfig::GetDefaultAnisotropy() == 8);

    // Test clamping
    TextureConfig::SetDefaultAnisotropy(0);
    assert(TextureConfig::GetDefaultAnisotropy() == 1);

    TextureConfig::SetDefaultAnisotropy(32);
    assert(TextureConfig::GetDefaultAnisotropy() == 16);

    // Reset to default
    TextureConfig::SetDefaultAnisotropy(16);
}

// Test TextureManager singleton
void TestTextureManagerSingleton() {
    TextureManager& tm1 = TextureManager::Instance();
    TextureManager& tm2 = TextureManager::Instance();

    // Both should be the same instance
    assert(&tm1 == &tm2);
}

// Test creating solid color textures
void TestCreateSolid() {
    TextureManager& tm = TextureManager::Instance();

    Vec4 red(1.0f, 0.0f, 0.0f, 1.0f);
    TextureHandle handle = tm.CreateSolid(4, 4, red, TextureUsage::Albedo);

    assert(handle.IsValid());
    assert(handle != TextureHandle::Invalid);

    // Verify texture data
    const TextureData* data = tm.Get(handle);
    assert(data != nullptr);
    assert(data->width == 4);
    assert(data->height == 4);
    assert(data->channels == 4);
    assert(data->usage == TextureUsage::Albedo);
    assert(data->pixels != nullptr);

    // Check pixel values (red = 255, green/blue = 0, alpha = 255)
    assert(data->pixels[0] == 255);  // R
    assert(data->pixels[1] == 0);    // G
    assert(data->pixels[2] == 0);    // B
    assert(data->pixels[3] == 255);  // A

    tm.Destroy(handle);
}

// Test creating default textures
void TestDefaultTextures() {
    TextureManager& tm = TextureManager::Instance();

    // Create white texture
    TextureHandle white = tm.CreateWhite();
    assert(white.IsValid());
    const TextureData* whiteData = tm.Get(white);
    assert(whiteData != nullptr);
    assert(whiteData->width == 1);
    assert(whiteData->height == 1);
    assert(whiteData->pixels[0] == 255);
    assert(whiteData->pixels[1] == 255);
    assert(whiteData->pixels[2] == 255);
    assert(whiteData->pixels[3] == 255);

    // Create black texture
    TextureHandle black = tm.CreateBlack();
    assert(black.IsValid());
    const TextureData* blackData = tm.Get(black);
    assert(blackData != nullptr);
    assert(blackData->width == 1);
    assert(blackData->height == 1);
    assert(blackData->pixels[0] == 0);
    assert(blackData->pixels[1] == 0);
    assert(blackData->pixels[2] == 0);
    assert(blackData->pixels[3] == 255);

    // Create normal map texture
    TextureHandle normal = tm.CreateNormalMap();
    assert(normal.IsValid());
    const TextureData* normalData = tm.Get(normal);
    assert(normalData != nullptr);
    assert(normalData->width == 1);
    assert(normalData->height == 1);
    assert(normalData->usage == TextureUsage::Normal);
    assert(normalData->pixels[0] == 127);  // X = 0 → 127
    assert(normalData->pixels[1] == 127);  // Y = 0 → 127
    assert(normalData->pixels[2] == 255);  // Z = 1 → 255
    assert(normalData->pixels[3] == 255);  // A = 1 → 255

    // Test caching: calling again should return same handle
    TextureHandle white2 = tm.CreateWhite();
    assert(white2 == white);

    TextureHandle black2 = tm.CreateBlack();
    assert(black2 == black);

    TextureHandle normal2 = tm.CreateNormalMap();
    assert(normal2 == normal);
}

// Test handle validity
void TestHandleValidity() {
    TextureManager& tm = TextureManager::Instance();

    TextureHandle invalid = TextureHandle::Invalid;
    assert(!tm.IsValid(invalid));

    Vec4 color(0.5f, 0.5f, 0.5f, 1.0f);
    TextureHandle valid = tm.CreateSolid(2, 2, color);
    assert(tm.IsValid(valid));

    tm.Destroy(valid);
    assert(!tm.IsValid(valid));
}

// Test resource count tracking
void TestResourceCount() {
    TextureManager& tm = TextureManager::Instance();

    size_t initialCount = tm.Count();

    Vec4 color(1.0f, 1.0f, 1.0f, 1.0f);
    TextureHandle h1 = tm.CreateSolid(2, 2, color);
    assert(tm.Count() == initialCount + 1);

    TextureHandle h2 = tm.CreateSolid(2, 2, color);
    assert(tm.Count() == initialCount + 2);

    tm.Destroy(h1);
    assert(tm.Count() == initialCount + 1);

    tm.Destroy(h2);
    assert(tm.Count() == initialCount);
}

i32 main() {
    std::cout << "=== Texture Manager Tests ===" << std::endl << std::endl;

    TEST(TestGlobalAnisotropyConfig);
    TEST(TestTextureManagerSingleton);
    TEST(TestCreateSolid);
    TEST(TestDefaultTextures);
    TEST(TestHandleValidity);
    TEST(TestResourceCount);

    std::cout << "All tests passed!" << std::endl;
    return 0;
}
