#pragma once

#include "core/types.h"

#include <vulkan/vulkan.h>

class VulkanContext;

// Offscreen render target for viewport rendering
// Manages color (HDR) and depth textures, render pass, and framebuffer
class VulkanRenderTarget {
public:
    VulkanRenderTarget() = default;
    ~VulkanRenderTarget();

    // Create render target with specified dimensions
    // Uses RGBA16F for color (HDR) and D32_SFLOAT for depth
    void Create(VulkanContext* context, u32 width, u32 height);

    // Resize render target (destroys and recreates resources)
    void Resize(u32 width, u32 height);

    // Destroy all Vulkan resources
    void Destroy();

    // Accessors
    VkImageView GetColorImageView() const { return m_ColorImageView; }
    VkImageView GetDepthImageView() const { return m_DepthImageView; }
    VkSampler GetColorSampler() const { return m_ColorSampler; }
    VkFramebuffer GetFramebuffer() const { return m_Framebuffer; }
    VkRenderPass GetRenderPass() const { return m_RenderPass; }
    VkFormat GetColorFormat() const { return m_ColorFormat; }
    VkFormat GetDepthFormat() const { return m_DepthFormat; }
    u32 GetWidth() const { return m_Width; }
    u32 GetHeight() const { return m_Height; }

    bool IsValid() const {
        return m_ColorImage != VK_NULL_HANDLE &&
               m_DepthImage != VK_NULL_HANDLE &&
               m_Framebuffer != VK_NULL_HANDLE &&
               m_RenderPass != VK_NULL_HANDLE;
    }

    // Prevent copying
    VulkanRenderTarget(const VulkanRenderTarget&) = delete;
    VulkanRenderTarget& operator=(const VulkanRenderTarget&) = delete;

private:
    void CreateColorResources();
    void CreateDepthResources();
    void CreateRenderPass();
    void CreateFramebuffer();
    void CreateSampler();
    void DestroyResources();

    VkFormat FindDepthFormat() const;
    u32 FindMemoryType(u32 typeFilter, VkMemoryPropertyFlags properties) const;

    VulkanContext* m_Context = nullptr;

    u32 m_Width = 0;
    u32 m_Height = 0;

    // Color attachment (HDR)
    VkImage m_ColorImage = VK_NULL_HANDLE;
    VkDeviceMemory m_ColorImageMemory = VK_NULL_HANDLE;
    VkImageView m_ColorImageView = VK_NULL_HANDLE;
    VkSampler m_ColorSampler = VK_NULL_HANDLE;
    VkFormat m_ColorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

    // Depth attachment
    VkImage m_DepthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_DepthImageMemory = VK_NULL_HANDLE;
    VkImageView m_DepthImageView = VK_NULL_HANDLE;
    VkFormat m_DepthFormat = VK_FORMAT_UNDEFINED;

    // Render pass and framebuffer
    VkRenderPass m_RenderPass = VK_NULL_HANDLE;
    VkFramebuffer m_Framebuffer = VK_NULL_HANDLE;
};
