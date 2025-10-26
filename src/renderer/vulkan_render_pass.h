#pragma once

#include "core/types.h"

#include <vulkan/vulkan.h>

class VulkanContext;
class VulkanSwapchain;

class VulkanRenderPass {
public:
    VulkanRenderPass() = default;
    ~VulkanRenderPass();

    void Init(VulkanContext* context, VulkanSwapchain* swapchain);
    void Shutdown();

    VkRenderPass Get() const { return m_RenderPass; }

private:
    VulkanContext* m_Context = nullptr;
    VkRenderPass m_RenderPass = VK_NULL_HANDLE;
};
