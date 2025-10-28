#include "core/texture_data.h"
#include <iostream>
#include <cassert>
#include <cstring>

#define TEST(name) \
    std::cout << "Running test: " << #name << "..." << std::endl; \
    name(); \
    std::cout << "  PASSED\n" << std::endl;

// Test array texture creation with layerPixels
void TestArrayTextureCreation() {
    TextureData data;
    data.width = 64;
    data.height = 64;
    data.channels = 4;
    data.arrayLayers = 3;
    data.type = TextureType::TextureArray;

    // Allocate 3 layers
    const u64 layerSize = 64 * 64 * 4;
    for (u32 i = 0; i < 3; ++i) {
        u8* layerData = static_cast<u8*>(malloc(layerSize));
        memset(layerData, i * 50, layerSize);  // Fill with different patterns
        data.layerPixels.push_back(layerData);
    }

    assert(data.layerPixels.size() == 3);
    assert(data.arrayLayers == 3);
    assert(data.type == TextureType::TextureArray);

    // Cleanup manually since we're testing without packing
    for (u8* layerData : data.layerPixels) {
        free(layerData);
    }
    data.layerPixels.clear();
}

// Test ValidateLayers with matching dimensions
void TestValidateLayersSuccess() {
    TextureData data;
    data.width = 128;
    data.height = 128;
    data.channels = 4;
    data.arrayLayers = 2;

    const u64 layerSize = 128 * 128 * 4;
    for (u32 i = 0; i < 2; ++i) {
        data.layerPixels.push_back(static_cast<u8*>(malloc(layerSize)));
    }

    assert(data.ValidateLayers() == true);

    // Cleanup
    for (u8* layerData : data.layerPixels) {
        free(layerData);
    }
    data.layerPixels.clear();
}

// Test ValidateLayers with mismatched count
void TestValidateLayersMismatchedCount() {
    TextureData data;
    data.width = 64;
    data.height = 64;
    data.channels = 4;
    data.arrayLayers = 3;

    // Only allocate 2 layers when 3 are expected
    const u64 layerSize = 64 * 64 * 4;
    for (u32 i = 0; i < 2; ++i) {
        data.layerPixels.push_back(static_cast<u8*>(malloc(layerSize)));
    }

    assert(data.ValidateLayers() == false);

    // Cleanup
    for (u8* layerData : data.layerPixels) {
        free(layerData);
    }
    data.layerPixels.clear();
}

// Test ValidateLayers with null pointer
void TestValidateLayersNullPointer() {
    TextureData data;
    data.width = 64;
    data.height = 64;
    data.channels = 4;
    data.arrayLayers = 2;

    const u64 layerSize = 64 * 64 * 4;
    data.layerPixels.push_back(static_cast<u8*>(malloc(layerSize)));
    data.layerPixels.push_back(nullptr);  // Invalid null layer

    assert(data.ValidateLayers() == false);

    // Cleanup
    if (data.layerPixels[0]) {
        free(data.layerPixels[0]);
    }
    data.layerPixels.clear();
}

// Test PackLayersIntoStagingBuffer basic functionality
void TestPackLayersBasic() {
    TextureData data;
    data.width = 32;
    data.height = 32;
    data.channels = 4;
    data.arrayLayers = 2;

    const u64 layerSize = 32 * 32 * 4;

    // Create two layers with distinct patterns
    u8* layer0 = static_cast<u8*>(malloc(layerSize));
    u8* layer1 = static_cast<u8*>(malloc(layerSize));
    memset(layer0, 0xAA, layerSize);
    memset(layer1, 0xBB, layerSize);

    data.layerPixels.push_back(layer0);
    data.layerPixels.push_back(layer1);

    // Pack layers
    bool success = data.PackLayersIntoStagingBuffer();
    assert(success == true);
    assert(data.pixels != nullptr);
    assert(data.layerPixels.empty());  // Should be cleared after packing

    // Verify data integrity
    const u64 totalSize = layerSize * 2;
    u8* packed = data.pixels;

    // Check layer 0 pattern
    for (u64 i = 0; i < layerSize; ++i) {
        assert(packed[i] == 0xAA);
    }

    // Check layer 1 pattern
    for (u64 i = layerSize; i < totalSize; ++i) {
        assert(packed[i] == 0xBB);
    }

    // Cleanup (TextureData destructor will handle it, but be explicit)
    // data.pixels will be freed by ~TextureData
}

// Test PackLayersIntoStagingBuffer with empty layerPixels
void TestPackLayersEmpty() {
    TextureData data;
    data.width = 64;
    data.height = 64;
    data.channels = 4;
    data.arrayLayers = 0;

    bool success = data.PackLayersIntoStagingBuffer();
    assert(success == true);  // Should succeed but do nothing
    assert(data.pixels == nullptr);
}

// Test PackLayersIntoStagingBuffer with validation failure
void TestPackLayersValidationFailure() {
    TextureData data;
    data.width = 64;
    data.height = 64;
    data.channels = 4;
    data.arrayLayers = 2;

    // Add only one layer (validation should fail)
    const u64 layerSize = 64 * 64 * 4;
    data.layerPixels.push_back(static_cast<u8*>(malloc(layerSize)));

    bool success = data.PackLayersIntoStagingBuffer();
    assert(success == false);  // Should fail validation

    // Cleanup
    if (!data.layerPixels.empty() && data.layerPixels[0]) {
        free(data.layerPixels[0]);
    }
    data.layerPixels.clear();
}

// Test array texture with mipmaps
void TestArrayTextureWithMipmaps() {
    TextureData data;
    data.width = 256;
    data.height = 256;
    data.channels = 4;
    data.arrayLayers = 4;
    data.mipLevels = 5;  // 256 -> 128 -> 64 -> 32 -> 16
    data.type = TextureType::TextureArray;

    const u64 layerSize = 256 * 256 * 4;
    for (u32 i = 0; i < 4; ++i) {
        data.layerPixels.push_back(static_cast<u8*>(malloc(layerSize)));
    }

    assert(data.ValidateLayers() == true);
    assert(data.arrayLayers == 4);
    assert(data.mipLevels == 5);

    bool success = data.PackLayersIntoStagingBuffer();
    assert(success == true);
    assert(data.pixels != nullptr);

    // Note: pixels only contains base mip, GPU generates other mips
    // Packed size = layerSize * 4 (4 layers at base mip level)
}

// Test move semantics with array textures
void TestArrayTextureMoveSemantics() {
    TextureData data1;
    data1.width = 64;
    data1.height = 64;
    data1.channels = 4;
    data1.arrayLayers = 2;

    const u64 layerSize = 64 * 64 * 4;
    data1.layerPixels.push_back(static_cast<u8*>(malloc(layerSize)));
    data1.layerPixels.push_back(static_cast<u8*>(malloc(layerSize)));

    data1.PackLayersIntoStagingBuffer();
    u8* originalPixels = data1.pixels;

    // Move construct
    TextureData data2(std::move(data1));
    assert(data2.pixels == originalPixels);
    assert(data2.arrayLayers == 2);
    assert(data1.pixels == nullptr);  // Moved-from state

    // Move assign
    TextureData data3;
    data3 = std::move(data2);
    assert(data3.pixels == originalPixels);
    assert(data3.arrayLayers == 2);
    assert(data2.pixels == nullptr);
}

int main() {
    std::cout << "=== TextureData Array Tests ===" << std::endl;

    TEST(TestArrayTextureCreation);
    TEST(TestValidateLayersSuccess);
    TEST(TestValidateLayersMismatchedCount);
    TEST(TestValidateLayersNullPointer);
    TEST(TestPackLayersBasic);
    TEST(TestPackLayersEmpty);
    TEST(TestPackLayersValidationFailure);
    TEST(TestArrayTextureWithMipmaps);
    TEST(TestArrayTextureMoveSemantics);

    std::cout << "All tests passed!" << std::endl;
    return 0;
}
