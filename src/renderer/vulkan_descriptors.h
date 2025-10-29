#pragma once

#include "core/types.h"
#include "renderer/vulkan_buffer.h"

#include <vulkan/vulkan.h>

#include <vector>
#include <queue>

class VulkanContext;

class VulkanDescriptors {
public:
    VulkanDescriptors() = default;
    ~VulkanDescriptors();

    // Configuration for bindless texture array size
    static constexpr u32 MAX_BINDLESS_TEXTURES = 4096;

    void Init(VulkanContext* context, u32 framesInFlight);
    void Shutdown();

    void CreateUniformBuffers(u32 framesInFlight);
    void UpdateUniformBuffer(u32 currentFrame, const void* data, size_t size);

    // Bindless texture registration
    // Returns descriptor index for use in shaders
    u32 RegisterTexture(VkImageView imageView, VkSampler sampler);

    // Free a texture descriptor index for reuse
    void UnregisterTexture(u32 descriptorIndex);

    // Bind material SSBO (binding 1)
    void BindMaterialBuffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);

    VkDescriptorSetLayout GetLayout() const { return m_DescriptorSetLayout; }
    VkDescriptorSet GetDescriptorSet(u32 frame) const { return m_DescriptorSets[frame]; }

private:
    VulkanContext* m_Context = nullptr;

    VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_DescriptorSets;

    std::vector<VulkanBuffer> m_UniformBuffers;

    // Bindless texture index management
    u32 m_NextTextureIndex = 0;
    std::queue<u32> m_FreeTextureIndices;

    void CreateDescriptorSetLayout();
    void CreateDescriptorPool(u32 framesInFlight);
    void CreateDescriptorSets(u32 framesInFlight);
};

