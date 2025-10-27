#pragma once

#include "core/types.h"

#include <vulkan/vulkan.h>

class VulkanContext;
class VulkanSwapchain;

class VulkanDepthBuffer {
public:
    VulkanDepthBuffer() = default;
    ~VulkanDepthBuffer();

    void Init(VulkanContext* context, VulkanSwapchain* swapchain);
    void Shutdown();
    void Recreate(VulkanSwapchain* swapchain);

    VkImageView GetImageView() const { return m_DepthImageView; }
    VkFormat GetFormat() const { return m_DepthFormat; }

private:
    VkFormat FindDepthFormat() const;
    void CreateDepthResources(VulkanSwapchain* swapchain);
    u32 FindMemoryType(u32 typeFilter, VkMemoryPropertyFlags properties) const;
    void DestroyResources();

    VulkanContext* m_Context = nullptr;

    VkImage m_DepthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_DepthImageMemory = VK_NULL_HANDLE;
    VkImageView m_DepthImageView = VK_NULL_HANDLE;
    VkFormat m_DepthFormat = VK_FORMAT_UNDEFINED;
};
