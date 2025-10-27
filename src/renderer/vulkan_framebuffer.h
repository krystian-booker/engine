#pragma once

#include "core/types.h"

#include <vulkan/vulkan.h>

#include <vector>

class VulkanContext;
class VulkanSwapchain;
class VulkanRenderPass;

// Owns the VkFramebuffer objects for each swapchain image.
class VulkanFramebuffer {
public:
    VulkanFramebuffer() = default;
    ~VulkanFramebuffer();

    void Init(
        VulkanContext* context,
        VulkanSwapchain* swapchain,
        VulkanRenderPass* renderPass,
        VkImageView depthImageView);
    void Shutdown();
    void Recreate(
        VulkanSwapchain* swapchain,
        VulkanRenderPass* renderPass,
        VkImageView depthImageView);

    VkFramebuffer Get(u32 index) const;
    const std::vector<VkFramebuffer>& GetFramebuffers() const { return m_Framebuffers; }
    u32 GetCount() const { return static_cast<u32>(m_Framebuffers.size()); }

private:
    void CreateFramebuffers(
        VulkanSwapchain* swapchain,
        VulkanRenderPass* renderPass,
        VkImageView depthImageView);

    VulkanContext* m_Context = nullptr;
    std::vector<VkFramebuffer> m_Framebuffers;
};
