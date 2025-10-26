#include "renderer/vulkan_context.h"
#include "renderer/vulkan_swapchain.h"
#include "platform/window.h"

#include <iostream>
#include <limits>
#include <stdexcept>

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

struct VulkanSwapchainTestAccess {
    static VkSurfaceFormatKHR ChooseSurfaceFormat(
        VulkanSwapchain& swapchain,
        const std::vector<VkSurfaceFormatKHR>& formats) {
        return swapchain.ChooseSwapSurfaceFormat(formats);
    }

    static VkPresentModeKHR ChoosePresentMode(
        VulkanSwapchain& swapchain,
        const std::vector<VkPresentModeKHR>& modes) {
        return swapchain.ChooseSwapPresentMode(modes);
    }

    static VkExtent2D ChooseExtent(
        VulkanSwapchain& swapchain,
        const VkSurfaceCapabilitiesKHR& capabilities,
        Window* window) {
        return swapchain.ChooseSwapExtent(capabilities, window);
    }
};

TEST(VulkanContext_InitAndShutdown) {
    WindowProperties props;
    props.title = "Vulkan Test";
    props.width = 640;
    props.height = 480;
    props.resizable = false;

    Window window(props);

    VulkanContext context;
    context.Init(&window);

    ASSERT(context.GetInstance() != VK_NULL_HANDLE);
    ASSERT(context.GetPhysicalDevice() != VK_NULL_HANDLE);
    ASSERT(context.GetDevice() != VK_NULL_HANDLE);
    ASSERT(context.GetGraphicsQueue() != VK_NULL_HANDLE);
    ASSERT(context.GetPresentQueue() != VK_NULL_HANDLE);
    ASSERT(context.GetSurface() != VK_NULL_HANDLE);
    ASSERT(context.GetGraphicsQueueFamily() != std::numeric_limits<u32>::max());
    ASSERT(context.GetPresentQueueFamily() != std::numeric_limits<u32>::max());

    context.Shutdown();

    ASSERT(context.GetInstance() == VK_NULL_HANDLE);
    ASSERT(context.GetDevice() == VK_NULL_HANDLE);
    ASSERT(context.GetSurface() == VK_NULL_HANDLE);
}

TEST(VulkanContext_DebugLayerToggle) {
    WindowProperties props;
    props.title = "Vulkan Debug Test";
    props.width = 320;
    props.height = 240;
    props.resizable = false;

    Window window(props);

    VulkanContext context;
    context.Init(&window);

#ifdef _DEBUG
    ASSERT(context.GetInstance() != VK_NULL_HANDLE);
#endif

    context.Shutdown();
}

TEST(VulkanSwapchain_ChooseSurfaceFormatPrefersSRGB) {
    VulkanSwapchain swapchain;

    std::vector<VkSurfaceFormatKHR> formats = {
        { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
        { VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
        { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR }
    };

    VkSurfaceFormatKHR chosen = VulkanSwapchainTestAccess::ChooseSurfaceFormat(swapchain, formats);
    ASSERT(chosen.format == VK_FORMAT_B8G8R8A8_SRGB);
    ASSERT(chosen.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
}

TEST(VulkanSwapchain_ChoosePresentModePrefersMailbox) {
    VulkanSwapchain swapchain;

    std::vector<VkPresentModeKHR> modesWithMailbox = {
        VK_PRESENT_MODE_FIFO_KHR,
        VK_PRESENT_MODE_MAILBOX_KHR,
        VK_PRESENT_MODE_IMMEDIATE_KHR
    };

    VkPresentModeKHR chosen = VulkanSwapchainTestAccess::ChoosePresentMode(swapchain, modesWithMailbox);
    ASSERT(chosen == VK_PRESENT_MODE_MAILBOX_KHR);

    std::vector<VkPresentModeKHR> modesWithoutMailbox = {
        VK_PRESENT_MODE_FIFO_KHR,
        VK_PRESENT_MODE_IMMEDIATE_KHR
    };

    VkPresentModeKHR fallback = VulkanSwapchainTestAccess::ChoosePresentMode(swapchain, modesWithoutMailbox);
    ASSERT(fallback == VK_PRESENT_MODE_FIFO_KHR);
}

TEST(VulkanSwapchain_ChooseExtentClampsToCapabilities) {
    VulkanSwapchain swapchain;

    WindowProperties props;
    props.title = "Swapchain Extent Test";
    props.width = 4000;
    props.height = 200;
    props.resizable = false;

    Window window(props);

    VkSurfaceCapabilitiesKHR capabilities{};
    capabilities.currentExtent.width = UINT32_MAX;
    capabilities.currentExtent.height = UINT32_MAX;
    capabilities.minImageExtent = { 640, 480 };
    capabilities.maxImageExtent = { 1920, 1080 };

    VkExtent2D extent = VulkanSwapchainTestAccess::ChooseExtent(swapchain, capabilities, &window);

    ASSERT(extent.width == capabilities.maxImageExtent.width);
    ASSERT(extent.height == capabilities.minImageExtent.height);
}

int main() {
    std::cout << "=== Vulkan Context Tests ===" << std::endl;
    std::cout << std::endl;

    VulkanContext_InitAndShutdown_runner();
    VulkanContext_DebugLayerToggle_runner();
    VulkanSwapchain_ChooseSurfaceFormatPrefersSRGB_runner();
    VulkanSwapchain_ChoosePresentModePrefersMailbox_runner();
    VulkanSwapchain_ChooseExtentClampsToCapabilities_runner();

    std::cout << std::endl;
    std::cout << "================================" << std::endl;
    std::cout << "Tests run: " << testsRun << std::endl;
    std::cout << "Tests passed: " << testsPassed << std::endl;
    std::cout << "Tests failed: " << testsFailed << std::endl;
    std::cout << "================================" << std::endl;

    return testsFailed == 0 ? 0 : 1;
}
