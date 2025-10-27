#pragma once
#include "core/types.h"
#include <vulkan/vulkan.h>

class VulkanContext;
struct TextureData;

// Vulkan texture abstraction (image, view, sampler)
class VulkanTexture {
public:
    VulkanTexture() = default;
    ~VulkanTexture();

    // Create texture from TextureData (uploads to GPU)
    void Create(VulkanContext* context, const TextureData* textureData);

    // Destroy all Vulkan resources
    void Destroy();

    // Accessors
    VkImage GetImage() const { return m_Image; }
    VkImageView GetImageView() const { return m_ImageView; }
    VkSampler GetSampler() const { return m_Sampler; }
    VkFormat GetFormat() const { return m_Format; }
    u32 GetMipLevels() const { return m_MipLevels; }

    bool IsValid() const {
        return m_Image != VK_NULL_HANDLE &&
               m_ImageView != VK_NULL_HANDLE &&
               m_Sampler != VK_NULL_HANDLE;
    }

    // Prevent copying
    VulkanTexture(const VulkanTexture&) = delete;
    VulkanTexture& operator=(const VulkanTexture&) = delete;

private:
    VulkanContext* m_Context = nullptr;

    VkImage m_Image = VK_NULL_HANDLE;
    VkDeviceMemory m_ImageMemory = VK_NULL_HANDLE;
    VkImageView m_ImageView = VK_NULL_HANDLE;
    VkSampler m_Sampler = VK_NULL_HANDLE;
    VkFormat m_Format = VK_FORMAT_UNDEFINED;
    u32 m_MipLevels = 1;

    // Helper functions
    void CreateImage(const TextureData* data);
    void CreateImageView();
    void CreateSampler(const TextureData* data);
    void GenerateMipmaps(VkImage image, VkFormat format, u32 width, u32 height, u32 mipLevels);
    void GenerateMipmapsBlit(VkImage image, VkFormat format, u32 width, u32 height, u32 mipLevels);
    void GenerateMipmapsCompute(VkImage image, VkFormat format, u32 width, u32 height, u32 mipLevels);
    void GenerateMipmapsCPU(VkImage image, VkFormat format, u32 width, u32 height, u32 mipLevels);
    void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout,
                               VkImageLayout newLayout, u32 mipLevels);
    void CopyBufferToImage(VkBuffer buffer, VkImage image, u32 width, u32 height);

    // Format detection
    VkFormat DetermineVulkanFormat(const TextureData* data) const;
    bool ShouldUseSRGB(const TextureData* data) const;

    // Utility
    u32 FindMemoryType(u32 typeFilter, VkMemoryPropertyFlags properties) const;
    VkCommandBuffer BeginSingleTimeCommands() const;
    void EndSingleTimeCommands(VkCommandBuffer commandBuffer) const;
};
