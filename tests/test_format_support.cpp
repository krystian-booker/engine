#include "renderer/vulkan_context.h"
#include "platform/window.h"
#include <iostream>
#include <vector>

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
// Format Support Tests
// ============================================================================

// Helper to create window properties for tests
static WindowProperties CreateTestWindowProps() {
    WindowProperties props;
    props.width = 800;
    props.height = 600;
    props.title = "Format Support Test";
    return props;
}

TEST(FormatSupport_CacheInitialization) {
    Window window(CreateTestWindowProps());
    VulkanContext context;
    context.Init(&window);

    // Verify that common formats are cached and queryable
    const VkFormatProperties* formatProps = context.GetFormatProperties(VK_FORMAT_R8G8B8A8_UNORM);
    ASSERT(formatProps != nullptr);

    context.Shutdown();
}

TEST(FormatSupport_LinearBlitDetection) {
    Window window(CreateTestWindowProps());
    VulkanContext context;
    context.Init(&window);

    // Most GPUs support linear blit for RGBA8 UNORM
    bool supportsLinearBlit = context.SupportsLinearBlit(VK_FORMAT_R8G8B8A8_UNORM);
    std::cout << "(RGBA8_UNORM linear blit: " << (supportsLinearBlit ? "yes" : "no") << ") ";

    // Test should not crash even if unsupported
    ASSERT(true);

    context.Shutdown();
}

TEST(FormatSupport_ColorAttachment) {
    Window window(CreateTestWindowProps());
    VulkanContext context;
    context.Init(&window);

    // RGBA8 should support color attachment on all GPUs
    bool supportsColorAttachment = context.SupportsColorAttachment(VK_FORMAT_R8G8B8A8_UNORM);
    std::cout << "(RGBA8_UNORM color attachment: " << (supportsColorAttachment ? "yes" : "no") << ") ";
    ASSERT(supportsColorAttachment);

    context.Shutdown();
}

TEST(FormatSupport_DepthStencilAttachment) {
    Window window(CreateTestWindowProps());
    VulkanContext context;
    context.Init(&window);

    // D32_SFLOAT should support depth attachment on most GPUs
    bool supportsDepth = context.SupportsDepthStencilAttachment(VK_FORMAT_D32_SFLOAT);
    std::cout << "(D32_SFLOAT depth attachment: " << (supportsDepth ? "yes" : "no") << ") ";

    // Test should not crash even if unsupported
    ASSERT(true);

    context.Shutdown();
}

TEST(FormatSupport_TransferOperations) {
    Window window(CreateTestWindowProps());
    VulkanContext context;
    context.Init(&window);

    // RGBA8 should support transfer operations
    bool supportsTransferSrc = context.SupportsTransferSrc(VK_FORMAT_R8G8B8A8_UNORM);
    bool supportsTransferDst = context.SupportsTransferDst(VK_FORMAT_R8G8B8A8_UNORM);

    std::cout << "(RGBA8_UNORM transfer src/dst: " << (supportsTransferSrc ? "yes" : "no")
              << "/" << (supportsTransferDst ? "yes" : "no") << ") ";

    ASSERT(supportsTransferSrc);
    ASSERT(supportsTransferDst);

    context.Shutdown();
}

TEST(FormatSupport_SampledImage) {
    Window window(CreateTestWindowProps());
    VulkanContext context;
    context.Init(&window);

    // RGBA8 should support sampled image
    bool supportsSampledImage = context.SupportsSampledImage(VK_FORMAT_R8G8B8A8_UNORM);
    std::cout << "(RGBA8_UNORM sampled image: " << (supportsSampledImage ? "yes" : "no") << ") ";
    ASSERT(supportsSampledImage);

    context.Shutdown();
}

TEST(FormatSupport_StorageImage) {
    Window window(CreateTestWindowProps());
    VulkanContext context;
    context.Init(&window);

    // Storage image support varies by GPU
    bool supportsStorageImage = context.SupportsStorageImage(VK_FORMAT_R8G8B8A8_UNORM);
    std::cout << "(RGBA8_UNORM storage image: " << (supportsStorageImage ? "yes" : "no") << ") ";

    // Test should not crash even if unsupported
    ASSERT(true);

    context.Shutdown();
}

TEST(FormatSupport_OnDemandCaching) {
    Window window(CreateTestWindowProps());
    VulkanContext context;
    context.Init(&window);

    // Query a format that's not in the initial cache
    // This should trigger on-demand caching
    const VkFormatProperties* props = context.GetFormatProperties(VK_FORMAT_R16_SFLOAT);
    ASSERT(props != nullptr);

    // Second query should hit the cache
    const VkFormatProperties* props2 = context.GetFormatProperties(VK_FORMAT_R16_SFLOAT);
    ASSERT(props2 != nullptr);
    ASSERT(props == props2); // Should be same pointer (cached)

    context.Shutdown();
}

TEST(FormatSupport_SRGBFormats) {
    Window window(CreateTestWindowProps());
    VulkanContext context;
    context.Init(&window);

    // Check both SRGB and UNORM variants
    bool supportsRGBA8SRGB = context.SupportsSampledImage(VK_FORMAT_R8G8B8A8_SRGB);
    bool supportsRGBA8UNORM = context.SupportsSampledImage(VK_FORMAT_R8G8B8A8_UNORM);

    std::cout << "(RGBA8 SRGB/UNORM sampled: " << (supportsRGBA8SRGB ? "yes" : "no")
              << "/" << (supportsRGBA8UNORM ? "yes" : "no") << ") ";

    ASSERT(supportsRGBA8UNORM);
    // SRGB support varies, but most GPUs support it
    (void)supportsRGBA8SRGB;

    context.Shutdown();
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "=== Format Support Detection Tests ===" << std::endl;
    std::cout << std::endl;

    FormatSupport_CacheInitialization_runner();
    FormatSupport_LinearBlitDetection_runner();
    FormatSupport_ColorAttachment_runner();
    FormatSupport_DepthStencilAttachment_runner();
    FormatSupport_TransferOperations_runner();
    FormatSupport_SampledImage_runner();
    FormatSupport_StorageImage_runner();
    FormatSupport_OnDemandCaching_runner();
    FormatSupport_SRGBFormats_runner();

    std::cout << std::endl;
    std::cout << "=== Test Summary ===" << std::endl;
    std::cout << "Total:  " << testsRun << std::endl;
    std::cout << "Passed: " << testsPassed << std::endl;
    std::cout << "Failed: " << testsFailed << std::endl;

    return (testsFailed == 0) ? 0 : 1;
}
