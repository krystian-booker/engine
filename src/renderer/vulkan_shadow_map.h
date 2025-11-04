#pragma once

#include "core/types.h"
#include <vulkan/vulkan.h>
#include <vector>

class VulkanContext;

// Shadow map configuration
struct ShadowMapConfig {
    u32 resolution = 2048;           // Shadow map resolution (per cascade/face)
    u32 numCascades = 4;             // Number of cascades for CSM (directional lights)
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
};

// Shadow map render target for depth-only rendering
// Supports both single shadow maps (point/spot) and cascaded shadow maps (directional)
class VulkanShadowMap {
public:
    VulkanShadowMap() = default;
    ~VulkanShadowMap();

    // Create single shadow map (for point/spot lights)
    void CreateSingle(VulkanContext* context, u32 resolution);

    // Create cascaded shadow map array (for directional lights)
    void CreateCascaded(VulkanContext* context, u32 resolution, u32 numCascades);

    // Create cubemap shadow map (for point lights)
    void CreateCubemap(VulkanContext* context, u32 resolution);

    // Destroy all Vulkan resources
    void Destroy();

    // Accessors
    VkImage GetDepthImage() const { return m_DepthImage; }
    VkImageView GetDepthImageView() const { return m_DepthImageView; }
    VkImageView GetCascadeImageView(u32 cascade) const;
    VkSampler GetSampler() const { return m_Sampler; }  // Comparison sampler for PCF
    VkSampler GetRawDepthSampler() const { return m_RawDepthSampler; }  // Non-comparison sampler for PCSS
    VkFramebuffer GetFramebuffer(u32 cascade = 0) const;
    VkRenderPass GetRenderPass() const { return m_RenderPass; }
    VkFormat GetDepthFormat() const { return m_DepthFormat; }
    u32 GetResolution() const { return m_Resolution; }
    u32 GetNumCascades() const { return m_NumCascades; }

    bool IsValid() const {
        return m_DepthImage != VK_NULL_HANDLE &&
               m_RenderPass != VK_NULL_HANDLE &&
               !m_Framebuffers.empty();
    }

    bool IsCascaded() const { return m_NumCascades > 1; }
    bool IsCubemap() const { return m_IsCubemap; }

    // Prevent copying
    VulkanShadowMap(const VulkanShadowMap&) = delete;
    VulkanShadowMap& operator=(const VulkanShadowMap&) = delete;

private:
    void CreateDepthImage(u32 resolution, u32 arrayLayers);
    void CreateImageViews();
    void CreateRenderPass();
    void CreateFramebuffers();
    void CreateSampler();
    void DestroyResources();

    u32 FindMemoryType(u32 typeFilter, VkMemoryPropertyFlags properties) const;

    VulkanContext* m_Context = nullptr;

    u32 m_Resolution = 0;
    u32 m_NumCascades = 1;  // 1 for single shadow map, 4+ for CSM
    bool m_IsCubemap = false;  // True for cubemap shadow maps (point lights)

    // Depth image (2D for single, 2D array for cascaded, cube for point lights)
    VkImage m_DepthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_DepthImageMemory = VK_NULL_HANDLE;
    VkImageView m_DepthImageView = VK_NULL_HANDLE;  // Full array view for sampling
    std::vector<VkImageView> m_CascadeImageViews;   // Individual layer views for rendering
    VkSampler m_Sampler = VK_NULL_HANDLE;  // Comparison sampler for hardware PCF
    VkSampler m_RawDepthSampler = VK_NULL_HANDLE;  // Non-comparison sampler for raw depth access (PCSS)
    VkFormat m_DepthFormat = VK_FORMAT_D32_SFLOAT;

    // Render pass and framebuffers (one per cascade)
    VkRenderPass m_RenderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_Framebuffers;
};
