#include "renderer/vulkan_framebuffer.h"

#include "renderer/vulkan_context.h"
#include "renderer/vulkan_render_pass.h"
#include "renderer/vulkan_swapchain.h"

#include <stdexcept>

VulkanFramebuffer::~VulkanFramebuffer() {
    Shutdown();
}

void VulkanFramebuffer::Init(
    VulkanContext* context,
    VulkanSwapchain* swapchain,
    VulkanRenderPass* renderPass,
    const std::vector<VkImageView>& depthImageViews) {

    if (!context || !swapchain || !renderPass) {
        throw std::invalid_argument("VulkanFramebuffer::Init requires valid context, swapchain, and render pass");
    }

    Shutdown();

    m_Context = context;
    CreateFramebuffers(swapchain, renderPass, depthImageViews);
}

void VulkanFramebuffer::Shutdown() {
    if (!m_Context) {
        m_Framebuffers.clear();
        return;
    }

    VkDevice device = m_Context->GetDevice();
    for (VkFramebuffer& framebuffer : m_Framebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
            framebuffer = VK_NULL_HANDLE;
        }
    }

    m_Framebuffers.clear();
    m_Context = nullptr;
}

void VulkanFramebuffer::Recreate(
    VulkanSwapchain* swapchain,
    VulkanRenderPass* renderPass,
    const std::vector<VkImageView>& depthImageViews) {

    VulkanContext* context = m_Context;
    Shutdown();

    if (!context) {
        throw std::runtime_error("VulkanFramebuffer::Recreate called before initialization");
    }

    Init(context, swapchain, renderPass, depthImageViews);
}

VkFramebuffer VulkanFramebuffer::Get(u32 index) const {
    if (index >= m_Framebuffers.size()) {
        throw std::out_of_range("VulkanFramebuffer::Get index out of range");
    }

    return m_Framebuffers[index];
}

void VulkanFramebuffer::CreateFramebuffers(
    VulkanSwapchain* swapchain,
    VulkanRenderPass* renderPass,
    const std::vector<VkImageView>& depthImageViews) {

    const auto& imageViews = swapchain->GetImageViews();
    if (imageViews.empty()) {
        throw std::runtime_error("VulkanFramebuffer::CreateFramebuffers no swapchain image views");
    }

    if (imageViews.size() != depthImageViews.size()) {
        throw std::runtime_error("VulkanFramebuffer::CreateFramebuffers depth image count mismatch");
    }

    m_Framebuffers.resize(imageViews.size(), VK_NULL_HANDLE);

    VkDevice device = m_Context->GetDevice();

    for (size_t i = 0; i < imageViews.size(); ++i) {
        const VkImageView attachments[] = { imageViews[i], depthImageViews[i] };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass->Get();
        framebufferInfo.attachmentCount = ARRAY_COUNT(attachments);
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapchain->GetExtent().width;
        framebufferInfo.height = swapchain->GetExtent().height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &m_Framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan framebuffer");
        }
    }
}
