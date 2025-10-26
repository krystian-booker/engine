#pragma once

#include "core/types.h"

#include <vulkan/vulkan.h>

#include <vector>

class VulkanContext;
class VulkanSwapchain;
class VulkanRenderPass;

class VulkanFramebuffers {
public:
    VulkanFramebuffers() = default;
    ~VulkanFramebuffers();

    void Init(VulkanContext* context, VulkanSwapchain* swapchain, VulkanRenderPass* renderPass);
    void Shutdown();

    VkFramebuffer Get(u32 index) const { return m_Framebuffers[index]; }
    const std::vector<VkFramebuffer>& GetAll() const { return m_Framebuffers; }

private:
    VulkanContext* m_Context = nullptr;
    VulkanSwapchain* m_Swapchain = nullptr;
    VulkanRenderPass* m_RenderPass = nullptr;

    std::vector<VkFramebuffer> m_Framebuffers;
};
