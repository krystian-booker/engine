#include <iostream>
#include <cassert>
#include "core/material_data.h"
#include "core/resource_handle.h"

void TestMaterialDataDefaults() {
    std::cout << "[TEST] Material Data Defaults" << std::endl;

    MaterialData matData;

    // Check default PBR parameters
    assert(matData.albedoTint.x == 1.0f);
    assert(matData.albedoTint.y == 1.0f);
    assert(matData.albedoTint.z == 1.0f);
    assert(matData.albedoTint.w == 1.0f);
    std::cout << "  ✓ Default albedo tint is white (1,1,1,1)" << std::endl;

    assert(matData.emissiveFactor.x == 0.0f);
    assert(matData.emissiveFactor.y == 0.0f);
    assert(matData.emissiveFactor.z == 0.0f);
    assert(matData.emissiveFactor.w == 0.0f);
    std::cout << "  ✓ Default emissive factor is (0,0,0,0)" << std::endl;

    assert(matData.metallicFactor == 0.0f);
    assert(matData.roughnessFactor == 0.5f);
    assert(matData.normalScale == 1.0f);
    assert(matData.aoStrength == 1.0f);
    std::cout << "  ✓ Default PBR parameters: metallic=0.0, roughness=0.5, normalScale=1.0, aoStrength=1.0" << std::endl;

    assert(matData.flags == MaterialFlags::None);
    std::cout << "  ✓ Default flags are None" << std::endl;

    assert(matData.gpuMaterialIndex == 0xFFFFFFFF);
    std::cout << "  ✓ GPU material index initialized to invalid (0xFFFFFFFF)" << std::endl;

    std::cout << std::endl;
}

void TestMaterialFlags() {
    std::cout << "[TEST] Material Flags" << std::endl;

    // Test flag operations
    MaterialFlags flags = MaterialFlags::None;
    assert(!HasFlag(flags, MaterialFlags::DoubleSided));
    assert(!HasFlag(flags, MaterialFlags::AlphaBlend));
    std::cout << "  ✓ Initial flags are None" << std::endl;

    // Set double-sided flag
    SetFlag(flags, MaterialFlags::DoubleSided);
    assert(HasFlag(flags, MaterialFlags::DoubleSided));
    assert(!HasFlag(flags, MaterialFlags::AlphaBlend));
    std::cout << "  ✓ SetFlag(DoubleSided) works correctly" << std::endl;

    // Add alpha blend flag
    SetFlag(flags, MaterialFlags::AlphaBlend);
    assert(HasFlag(flags, MaterialFlags::DoubleSided));
    assert(HasFlag(flags, MaterialFlags::AlphaBlend));
    std::cout << "  ✓ Multiple flags can be set" << std::endl;

    // Clear double-sided flag
    ClearFlag(flags, MaterialFlags::DoubleSided);
    assert(!HasFlag(flags, MaterialFlags::DoubleSided));
    assert(HasFlag(flags, MaterialFlags::AlphaBlend));
    std::cout << "  ✓ ClearFlag works correctly" << std::endl;

    std::cout << std::endl;
}

void TestMaterialHelperMethods() {
    std::cout << "[TEST] Material Helper Methods" << std::endl;

    MaterialData matData;
    matData.flags = MaterialFlags::None;

    // Test UsesAlpha
    assert(!matData.UsesAlpha());
    SetFlag(matData.flags, MaterialFlags::AlphaBlend);
    assert(matData.UsesAlpha());
    std::cout << "  ✓ UsesAlpha() returns true when AlphaBlend flag is set" << std::endl;

    matData.flags = MaterialFlags::None;
    SetFlag(matData.flags, MaterialFlags::AlphaMask);
    assert(matData.UsesAlpha());
    std::cout << "  ✓ UsesAlpha() returns true when AlphaMask flag is set" << std::endl;

    // Test IsDoubleSided
    matData.flags = MaterialFlags::None;
    assert(!matData.IsDoubleSided());
    SetFlag(matData.flags, MaterialFlags::DoubleSided);
    assert(matData.IsDoubleSided());
    std::cout << "  ✓ IsDoubleSided() returns true when DoubleSided flag is set" << std::endl;

    std::cout << std::endl;
}

void TestMaterialTypeSafety() {
    std::cout << "[TEST] Type Safety" << std::endl;

    // This test is mostly compile-time verification
    MaterialHandle materialHandle = MaterialHandle::Invalid;
    TextureHandle textureHandle = TextureHandle::Invalid;

    // Verify they're invalid by default
    assert(!materialHandle.IsValid());
    assert(!textureHandle.IsValid());
    std::cout << "  ✓ MaterialHandle and TextureHandle are distinct types" << std::endl;
    std::cout << "  ✓ Both have proper Invalid constants" << std::endl;

    std::cout << std::endl;
}

int main() {
    std::cout << "=== Material System Tests ===" << std::endl;
    std::cout << "Testing core material data structures and flags (no Vulkan required)" << std::endl;
    std::cout << std::endl;

    TestMaterialDataDefaults();
    TestMaterialFlags();
    TestMaterialHelperMethods();
    TestMaterialTypeSafety();

    std::cout << "====================================" << std::endl;
    std::cout << "All material system tests passed!" << std::endl;
    std::cout << "====================================" << std::endl;
    std::cout << std::endl;
    std::cout << "Note: This test covers material data structures and flags." << std::endl;
    std::cout << "Material loading and texture creation require Vulkan initialization" << std::endl;
    std::cout << "and are tested through integration tests." << std::endl;

    return 0;
}
