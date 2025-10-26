#include "renderer/vulkan_framebuffers.h"

#include "renderer/vulkan_context.h"
#include "renderer/vulkan_render_pass.h"
#include "renderer/vulkan_swapchain.h"

#include <stdexcept>

VulkanFramebuffers::~VulkanFramebuffers() {
    Shutdown();
}

void VulkanFramebuffers::Init(VulkanContext* context, VulkanSwapchain* swapchain, VulkanRenderPass* renderPass) {
    if (!context || !swapchain || !renderPass) {
        throw std::invalid_argument("VulkanFramebuffers::Init requires valid context, swapchain, and render pass");
    }

    Shutdown();

    m_Context = context;
    m_Swapchain = swapchain;
    m_RenderPass = renderPass;
    m_Framebuffers.resize(m_Swapchain->GetImageCount(), VK_NULL_HANDLE);

    const auto& imageViews = m_Swapchain->GetImageViews();

    for (size_t i = 0; i < imageViews.size(); ++i) {
        VkImageView attachments[] = { imageViews[i] };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_RenderPass->Get();
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = m_Swapchain->GetExtent().width;
        framebufferInfo.height = m_Swapchain->GetExtent().height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(m_Context->GetDevice(), &framebufferInfo, nullptr, &m_Framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan framebuffer");
        }
    }
}

void VulkanFramebuffers::Shutdown() {
    if (m_Context) {
        for (auto& framebuffer : m_Framebuffers) {
            if (framebuffer != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(m_Context->GetDevice(), framebuffer, nullptr);
                framebuffer = VK_NULL_HANDLE;
            }
        }
    }

    m_Framebuffers.clear();
    m_Context = nullptr;
    m_Swapchain = nullptr;
    m_RenderPass = nullptr;
}
