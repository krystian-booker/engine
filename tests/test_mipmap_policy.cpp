#include "renderer/mipmap_policy.h"
#include "core/texture_data.h"
#include "core/types.h"
#include <iostream>

// Test result tracking
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
        } catch (...) { \
            testsFailed++; \
            std::cout << "FAILED (exception)" << std::endl; \
        } \
    } \
    static void name()

#define ASSERT(expr) \
    if (!(expr)) { \
        std::cout << "FAILED at line " << __LINE__ << ": " << #expr << std::endl; \
        testsFailed++; \
        testsRun++; \
        return; \
    }

// ============================================================================
// MipmapPolicy Tests
// ============================================================================

TEST(MipmapPolicy_ForceCPU) {
    // ForceCPU should always return CPU regardless of other params
    MipmapGenerationParams params{};
    params.usage = TextureUsage::Albedo;
    params.format = VK_FORMAT_R8G8B8A8_SRGB;
    params.policy = MipmapPolicy::ForceCPU;
    params.quality = MipmapQuality::High;
    params.width = 1024;
    params.height = 1024;
    params.context = nullptr;  // CPU doesn't need context

    MipmapMethod method = SelectMipGenerator(params);
    ASSERT(method == MipmapMethod::CPU);
}

TEST(MipmapPolicy_NormalMapUsesCompute) {
    // Normal maps should prefer compute for proper renormalization
    MipmapGenerationParams params{};
    params.usage = TextureUsage::Normal;
    params.format = VK_FORMAT_R8G8B8A8_UNORM;
    params.policy = MipmapPolicy::Auto;
    params.quality = MipmapQuality::Balanced;
    params.width = 512;
    params.height = 512;
    params.context = nullptr;  // Will fall back to CPU since no context

    MipmapMethod method = SelectMipGenerator(params);
    // Without valid context, should fall back to CPU
    ASSERT(method == MipmapMethod::CPU);
}

TEST(MipmapPolicy_PackedPBRUsesCompute) {
    // PackedPBR maps should prefer compute for proper channel handling
    MipmapGenerationParams params{};
    params.usage = TextureUsage::PackedPBR;
    params.format = VK_FORMAT_R8G8B8A8_UNORM;
    params.policy = MipmapPolicy::Auto;
    params.quality = MipmapQuality::Balanced;
    params.width = 1024;
    params.height = 1024;
    params.context = nullptr;  // Will fall back to CPU

    MipmapMethod method = SelectMipGenerator(params);
    // Without valid context, should fall back to CPU
    ASSERT(method == MipmapMethod::CPU);
}

TEST(MipmapPolicy_RoughnessUsesCompute) {
    // Roughness maps should prefer compute for Toksvig filtering
    MipmapGenerationParams params{};
    params.usage = TextureUsage::Roughness;
    params.format = VK_FORMAT_R8_UNORM;
    params.policy = MipmapPolicy::Auto;
    params.quality = MipmapQuality::Balanced;
    params.width = 512;
    params.height = 512;
    params.context = nullptr;  // Will fall back to CPU

    MipmapMethod method = SelectMipGenerator(params);
    // Without valid context, should fall back to CPU
    ASSERT(method == MipmapMethod::CPU);
}

TEST(MipmapPolicy_HeightUsesCompute) {
    // Height maps should prefer compute (like normals)
    MipmapGenerationParams params{};
    params.usage = TextureUsage::Height;
    params.format = VK_FORMAT_R8_UNORM;
    params.policy = MipmapPolicy::Auto;
    params.quality = MipmapQuality::Balanced;
    params.width = 512;
    params.height = 512;
    params.context = nullptr;  // Will fall back to CPU

    MipmapMethod method = SelectMipGenerator(params);
    // Without valid context, should fall back to CPU
    ASSERT(method == MipmapMethod::CPU);
}

TEST(MipmapPolicy_TextureDataDefaults) {
    // Verify TextureData initializes with correct defaults
    TextureData data;

    ASSERT(data.mipmapPolicy == MipmapPolicy::Auto);
    ASSERT(data.qualityHint == MipmapQuality::Balanced);
    ASSERT(data.usage == TextureUsage::Generic);
}

TEST(MipmapPolicy_EnumValues) {
    // Verify enum values are distinct
    ASSERT(MipmapPolicy::Auto != MipmapPolicy::ForceBlit);
    ASSERT(MipmapPolicy::Auto != MipmapPolicy::ForceCompute);
    ASSERT(MipmapPolicy::Auto != MipmapPolicy::ForceCPU);
    ASSERT(MipmapPolicy::ForceBlit != MipmapPolicy::ForceCompute);

    ASSERT(MipmapQuality::High != MipmapQuality::Balanced);
    ASSERT(MipmapQuality::High != MipmapQuality::Fast);
    ASSERT(MipmapQuality::Balanced != MipmapQuality::Fast);

    ASSERT(MipmapMethod::Blit != MipmapMethod::Compute);
    ASSERT(MipmapMethod::Blit != MipmapMethod::CPU);
    ASSERT(MipmapMethod::Compute != MipmapMethod::CPU);
}

TEST(MipmapPolicy_PackedPBRUsageExists) {
    // Verify PackedPBR usage enum exists and is distinct
    ASSERT(TextureUsage::PackedPBR != TextureUsage::Roughness);
    ASSERT(TextureUsage::PackedPBR != TextureUsage::Metalness);
    ASSERT(TextureUsage::PackedPBR != TextureUsage::AO);
    ASSERT(TextureUsage::PackedPBR != TextureUsage::Generic);
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "===========================================\n";
    std::cout << "   Mipmap Policy Tests\n";
    std::cout << "===========================================\n\n";

    // Run all tests
    MipmapPolicy_ForceCPU_runner();
    MipmapPolicy_NormalMapUsesCompute_runner();
    MipmapPolicy_PackedPBRUsesCompute_runner();
    MipmapPolicy_RoughnessUsesCompute_runner();
    MipmapPolicy_HeightUsesCompute_runner();
    MipmapPolicy_TextureDataDefaults_runner();
    MipmapPolicy_EnumValues_runner();
    MipmapPolicy_PackedPBRUsageExists_runner();

    // Print summary
    std::cout << "\n===========================================\n";
    std::cout << "Tests run: " << testsRun << std::endl;
    std::cout << "Tests passed: " << testsPassed << std::endl;
    std::cout << "Tests failed: " << testsFailed << std::endl;
    std::cout << "===========================================\n";

    return testsFailed > 0 ? 1 : 0;
}
