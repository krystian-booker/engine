#include "renderer/vulkan_context.h"
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

int main() {
    std::cout << "=== Vulkan Context Tests ===" << std::endl;
    std::cout << std::endl;

    VulkanContext_InitAndShutdown_runner();
    VulkanContext_DebugLayerToggle_runner();

    std::cout << std::endl;
    std::cout << "================================" << std::endl;
    std::cout << "Tests run: " << testsRun << std::endl;
    std::cout << "Tests passed: " << testsPassed << std::endl;
    std::cout << "Tests failed: " << testsFailed << std::endl;
    std::cout << "================================" << std::endl;

    return testsFailed == 0 ? 0 : 1;
}
