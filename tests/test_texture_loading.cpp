#include "core/texture_data.h"
#include "core/texture_load_options.h"
#include "resources/image_loader.h"
#include <iostream>
#include <cassert>

#define TEST(name) \
    std::cout << "Running test: " << #name << "..." << std::endl; \
    name(); \
    std::cout << "  PASSED\n" << std::endl;

// Test TextureUsage enum values
void TestTextureUsageEnum() {
    assert(static_cast<u8>(TextureUsage::Albedo) == 0);
    assert(static_cast<u8>(TextureUsage::Normal) == 1);
    assert(static_cast<u8>(TextureUsage::Roughness) == 2);
    assert(static_cast<u8>(TextureUsage::Metalness) == 3);
    assert(static_cast<u8>(TextureUsage::AO) == 4);
    assert(static_cast<u8>(TextureUsage::Height) == 5);
    assert(static_cast<u8>(TextureUsage::Generic) == 6);
}

// Test TextureFlags bitwise operations
void TestTextureFlagsBitwise() {
    TextureFlags flags = TextureFlags::None;

    // Test OR
    flags = flags | TextureFlags::SRGB;
    assert(HasFlag(flags, TextureFlags::SRGB));

    flags = flags | TextureFlags::GenerateMipmaps;
    assert(HasFlag(flags, TextureFlags::SRGB));
    assert(HasFlag(flags, TextureFlags::GenerateMipmaps));

    // Test AND
    TextureFlags combined = TextureFlags::SRGB | TextureFlags::GenerateMipmaps;
    TextureFlags result = combined & TextureFlags::SRGB;
    assert(HasFlag(result, TextureFlags::SRGB));

    // Test compound assignment
    flags = TextureFlags::None;
    flags |= TextureFlags::AnisotropyOverride;
    assert(HasFlag(flags, TextureFlags::AnisotropyOverride));
}

// Test TextureType enum
void TestTextureTypeEnum() {
    assert(static_cast<u8>(TextureType::Texture2D) == 0);
    assert(static_cast<u8>(TextureType::TextureArray) == 1);
    assert(static_cast<u8>(TextureType::Cubemap) == 2);
}

// Test TextureData construction and move semantics
void TestTextureDataMoveSemantics() {
    TextureData data1;
    data1.width = 256;
    data1.height = 256;
    data1.channels = 4;
    data1.pixels = static_cast<u8*>(malloc(256 * 256 * 4));

    // Move construction
    TextureData data2(std::move(data1));
    assert(data2.width == 256);
    assert(data2.height == 256);
    assert(data2.channels == 4);
    assert(data2.pixels != nullptr);
    assert(data1.pixels == nullptr);  // Moved-from should be null

    // Move assignment
    TextureData data3;
    data3 = std::move(data2);
    assert(data3.width == 256);
    assert(data3.pixels != nullptr);
    assert(data2.pixels == nullptr);
}

// Test TextureLoadOptions convenience constructors
void TestTextureLoadOptionsConvenience() {
    // Albedo
    auto albedoOpts = TextureLoadOptions::Albedo();
    assert(albedoOpts.usage == TextureUsage::Albedo);
    assert(albedoOpts.autoDetectSRGB == true);

    // Normal
    auto normalOpts = TextureLoadOptions::Normal();
    assert(normalOpts.usage == TextureUsage::Normal);

    // Roughness
    auto roughnessOpts = TextureLoadOptions::Roughness();
    assert(roughnessOpts.usage == TextureUsage::Roughness);
    assert(roughnessOpts.desiredChannels == 1);

    // Metalness
    auto metalnessOpts = TextureLoadOptions::Metalness();
    assert(metalnessOpts.usage == TextureUsage::Metalness);
    assert(metalnessOpts.desiredChannels == 1);

    // AO
    auto aoOpts = TextureLoadOptions::AO();
    assert(aoOpts.usage == TextureUsage::AO);
    assert(aoOpts.desiredChannels == 1);

    // Height
    auto heightOpts = TextureLoadOptions::Height();
    assert(heightOpts.usage == TextureUsage::Height);
    assert(heightOpts.desiredChannels == 1);
}

// Test ImageData validity check
void TestImageDataValidity() {
    ImageData invalid;
    assert(!invalid.IsValid());

    ImageData valid;
    valid.pixels = static_cast<u8*>(malloc(16));
    valid.width = 2;
    valid.height = 2;
    valid.channels = 4;
    assert(valid.IsValid());

    free(valid.pixels);
}

// Note: Actual image loading tests require test image files
// These will be tested in the build validation step

i32 main() {
    std::cout << "=== Texture Loading Tests ===" << std::endl << std::endl;

    TEST(TestTextureUsageEnum);
    TEST(TestTextureFlagsBitwise);
    TEST(TestTextureTypeEnum);
    TEST(TestTextureDataMoveSemantics);
    TEST(TestTextureLoadOptionsConvenience);
    TEST(TestImageDataValidity);

    std::cout << "All tests passed!" << std::endl;
    return 0;
}
