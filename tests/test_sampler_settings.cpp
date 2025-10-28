#include "core/sampler_settings.h"
#include "core/texture_load_options.h"
#include "core/texture_data.h"
#include <iostream>
#include <cassert>

#define TEST(name) \
    std::cout << "Running test: " << #name << "..." << std::endl; \
    name(); \
    std::cout << "  PASSED\n" << std::endl;

// Test SamplerSettings default constructor
void TestSamplerSettingsDefault() {
    SamplerSettings settings;

    assert(settings.magFilter == SamplerFilter::Linear);
    assert(settings.minFilter == SamplerFilter::Linear);
    assert(settings.addressModeU == SamplerAddressMode::Repeat);
    assert(settings.addressModeV == SamplerAddressMode::Repeat);
    assert(settings.addressModeW == SamplerAddressMode::Repeat);
    assert(settings.anisotropyEnable == true);
    assert(settings.maxAnisotropy == 16.0f);
    assert(settings.borderColor == SamplerBorderColor::OpaqueBlack);
    assert(settings.mipmapMode == SamplerMipmapMode::Linear);
    assert(settings.mipLodBias == 0.0f);
    assert(settings.minLod == 0.0f);
    assert(settings.maxLod == 1000.0f);
    assert(settings.compareEnable == false);
    assert(settings.unnormalizedCoordinates == false);
}

// Test SamplerSettings::Default() convenience constructor
void TestSamplerSettingsDefaultConstructor() {
    SamplerSettings settings = SamplerSettings::Default();

    assert(settings.magFilter == SamplerFilter::Linear);
    assert(settings.anisotropyEnable == true);
}

// Test SamplerSettings::Nearest() convenience constructor
void TestSamplerSettingsNearest() {
    SamplerSettings settings = SamplerSettings::Nearest();

    assert(settings.magFilter == SamplerFilter::Nearest);
    assert(settings.minFilter == SamplerFilter::Nearest);
    assert(settings.mipmapMode == SamplerMipmapMode::Nearest);
    assert(settings.anisotropyEnable == false);
}

// Test SamplerSettings::Clamped() convenience constructor
void TestSamplerSettingsClamped() {
    SamplerSettings settings = SamplerSettings::Clamped();

    assert(settings.addressModeU == SamplerAddressMode::ClampToEdge);
    assert(settings.addressModeV == SamplerAddressMode::ClampToEdge);
    assert(settings.addressModeW == SamplerAddressMode::ClampToEdge);
}

// Test SamplerSettings::Mirrored() convenience constructor
void TestSamplerSettingsMirrored() {
    SamplerSettings settings = SamplerSettings::Mirrored();

    assert(settings.addressModeU == SamplerAddressMode::MirroredRepeat);
    assert(settings.addressModeV == SamplerAddressMode::MirroredRepeat);
    assert(settings.addressModeW == SamplerAddressMode::MirroredRepeat);
}

// Test SamplerSettings::HighQuality() convenience constructor
void TestSamplerSettingsHighQuality() {
    SamplerSettings settings = SamplerSettings::HighQuality();

    assert(settings.maxAnisotropy == 16.0f);
    assert(settings.anisotropyEnable == true);
}

// Test SamplerSettings::LowQuality() convenience constructor
void TestSamplerSettingsLowQuality() {
    SamplerSettings settings = SamplerSettings::LowQuality();

    assert(settings.anisotropyEnable == false);
}

// Test SamplerSettings::Shadow() convenience constructor
void TestSamplerSettingsShadow() {
    SamplerSettings settings = SamplerSettings::Shadow();

    assert(settings.addressModeU == SamplerAddressMode::ClampToBorder);
    assert(settings.addressModeV == SamplerAddressMode::ClampToBorder);
    assert(settings.addressModeW == SamplerAddressMode::ClampToBorder);
    assert(settings.borderColor == SamplerBorderColor::OpaqueWhite);
    assert(settings.compareEnable == true);
    assert(settings.anisotropyEnable == false);
}

// Test TextureLoadOptions includes SamplerSettings
void TestTextureLoadOptionsHasSamplerSettings() {
    TextureLoadOptions options;

    // Should have default sampler settings
    assert(options.samplerSettings.magFilter == SamplerFilter::Linear);
    assert(options.samplerSettings.minFilter == SamplerFilter::Linear);
}

// Test custom SamplerSettings in TextureLoadOptions
void TestTextureLoadOptionsCustomSampler() {
    TextureLoadOptions options;
    options.samplerSettings = SamplerSettings::Nearest();

    assert(options.samplerSettings.magFilter == SamplerFilter::Nearest);
    assert(options.samplerSettings.minFilter == SamplerFilter::Nearest);
    assert(options.samplerSettings.anisotropyEnable == false);
}

// Test TextureData includes SamplerSettings
void TestTextureDataHasSamplerSettings() {
    TextureData data;

    // Should have default sampler settings
    assert(data.samplerSettings.magFilter == SamplerFilter::Linear);
    assert(data.samplerSettings.anisotropyEnable == true);
}

// Test custom SamplerSettings in TextureData
void TestTextureDataCustomSampler() {
    TextureData data;
    data.samplerSettings = SamplerSettings::Clamped();

    assert(data.samplerSettings.addressModeU == SamplerAddressMode::ClampToEdge);
    assert(data.samplerSettings.addressModeV == SamplerAddressMode::ClampToEdge);
}

// Test modifying individual sampler properties
void TestSamplerSettingsModification() {
    SamplerSettings settings;

    // Modify filtering
    settings.magFilter = SamplerFilter::Nearest;
    settings.minFilter = SamplerFilter::Nearest;
    assert(settings.magFilter == SamplerFilter::Nearest);
    assert(settings.minFilter == SamplerFilter::Nearest);

    // Modify wrapping
    settings.addressModeU = SamplerAddressMode::ClampToBorder;
    assert(settings.addressModeU == SamplerAddressMode::ClampToBorder);

    // Modify anisotropy
    settings.maxAnisotropy = 8.0f;
    assert(settings.maxAnisotropy == 8.0f);

    // Disable anisotropy
    settings.anisotropyEnable = false;
    assert(settings.anisotropyEnable == false);

    // Modify LOD
    settings.minLod = 1.0f;
    settings.maxLod = 5.0f;
    assert(settings.minLod == 1.0f);
    assert(settings.maxLod == 5.0f);
}

// Test combining preset with modifications
void TestSamplerSettingsCombinations() {
    // Start with clamped preset
    SamplerSettings settings = SamplerSettings::Clamped();

    // But use nearest filtering
    settings.magFilter = SamplerFilter::Nearest;
    settings.minFilter = SamplerFilter::Nearest;

    // Should have clamped wrapping with nearest filtering
    assert(settings.addressModeU == SamplerAddressMode::ClampToEdge);
    assert(settings.magFilter == SamplerFilter::Nearest);
}

// Test SamplerFilter enum values
void TestSamplerFilterEnum() {
    assert(static_cast<u8>(SamplerFilter::Nearest) == 0);
    assert(static_cast<u8>(SamplerFilter::Linear) == 1);
}

// Test SamplerAddressMode enum values
void TestSamplerAddressModeEnum() {
    assert(static_cast<u8>(SamplerAddressMode::Repeat) == 0);
    assert(static_cast<u8>(SamplerAddressMode::MirroredRepeat) == 1);
    assert(static_cast<u8>(SamplerAddressMode::ClampToEdge) == 2);
    assert(static_cast<u8>(SamplerAddressMode::ClampToBorder) == 3);
    assert(static_cast<u8>(SamplerAddressMode::MirrorClampToEdge) == 4);
}

// Test SamplerBorderColor enum values
void TestSamplerBorderColorEnum() {
    assert(static_cast<u8>(SamplerBorderColor::TransparentBlack) == 0);
    assert(static_cast<u8>(SamplerBorderColor::OpaqueBlack) == 1);
    assert(static_cast<u8>(SamplerBorderColor::OpaqueWhite) == 2);
}

// Test SamplerMipmapMode enum values
void TestSamplerMipmapModeEnum() {
    assert(static_cast<u8>(SamplerMipmapMode::Nearest) == 0);
    assert(static_cast<u8>(SamplerMipmapMode::Linear) == 1);
}

int main() {
    std::cout << "=== SamplerSettings Tests ===" << std::endl;

    TEST(TestSamplerSettingsDefault);
    TEST(TestSamplerSettingsDefaultConstructor);
    TEST(TestSamplerSettingsNearest);
    TEST(TestSamplerSettingsClamped);
    TEST(TestSamplerSettingsMirrored);
    TEST(TestSamplerSettingsHighQuality);
    TEST(TestSamplerSettingsLowQuality);
    TEST(TestSamplerSettingsShadow);
    TEST(TestTextureLoadOptionsHasSamplerSettings);
    TEST(TestTextureLoadOptionsCustomSampler);
    TEST(TestTextureDataHasSamplerSettings);
    TEST(TestTextureDataCustomSampler);
    TEST(TestSamplerSettingsModification);
    TEST(TestSamplerSettingsCombinations);
    TEST(TestSamplerFilterEnum);
    TEST(TestSamplerAddressModeEnum);
    TEST(TestSamplerBorderColorEnum);
    TEST(TestSamplerMipmapModeEnum);

    std::cout << "All tests passed!" << std::endl;
    return 0;
}
