#pragma once

#include "core/types.h"
#include "renderer/vulkan_swapchain.h"

#include <vulkan/vulkan.h>

#include <vector>

class VulkanContext;
class Window;

class VulkanRenderer {
public:
    VulkanRenderer() = default;
    ~VulkanRenderer();

    void Init(VulkanContext* context, Window* window);
    void Shutdown();

    void DrawFrame();
    void OnWindowResized();

private:
    void CreateRenderPass();
    void CreateFramebuffers();
    void CreateCommandPool();
    void CreateCommandBuffers();
    void CreateSyncObjects();
    void CleanupSwapchainResources();
    void RecreateSwapchain();
    void RecordCommandBuffer(VkCommandBuffer commandBuffer, u32 imageIndex);
    void ResizeImagesInFlight();

    VulkanContext* m_Context = nullptr;
    Window* m_Window = nullptr;

    VulkanSwapchain m_Swapchain;

    VkRenderPass m_RenderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_Framebuffers;

    VkCommandPool m_CommandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_CommandBuffers;

    static constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<VkSemaphore> m_ImageAvailableSemaphores;
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;
    std::vector<VkFence> m_InFlightFences;
    std::vector<VkFence> m_ImagesInFlight;

    u32 m_CurrentFrame = 0;
    bool m_FramebufferResized = false;
    bool m_Initialized = false;
};

