#pragma once

#include "core/types.h"

#include <vulkan/vulkan.h>
#include <vector>

class VulkanContext;
class VulkanSwapchain;

class VulkanRenderPass {
public:
    VulkanRenderPass() = default;
    ~VulkanRenderPass();

    void Init(VulkanContext* context, VulkanSwapchain* swapchain);
    void Shutdown();

    VkRenderPass Get() const { return m_RenderPass; }
    VkFormat GetDepthFormat() const { return m_DepthFormat; }

private:
    VkFormat FindDepthFormat(VulkanContext* context);
    VkFormat FindSupportedFormat(
        VulkanContext* context,
        const std::vector<VkFormat>& candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags features);

    VulkanContext* m_Context = nullptr;
    VkRenderPass m_RenderPass = VK_NULL_HANDLE;
    VkFormat m_DepthFormat = VK_FORMAT_UNDEFINED;
};
