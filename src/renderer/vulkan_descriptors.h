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
    void UpdateLightingBuffer(u32 currentFrame, const void* data, size_t size);

    // Shadow and IBL texture binding methods (Set 0, Transient)
    void BindShadowUBO(u32 currentFrame, const void* data, size_t size);  // Binding 2
    void BindShadowMap(VkImageView imageView, VkSampler sampler);  // Binding 3
    void BindIBLIrradiance(VkImageView imageView, VkSampler sampler);  // Binding 4
    void BindIBLPrefiltered(VkImageView imageView, VkSampler sampler);  // Binding 5
    void BindIBLBRDF(VkImageView imageView, VkSampler sampler);  // Binding 6
    void BindPointShadowMaps(VkImageView imageView, VkSampler sampler);  // Binding 7
    void BindSpotShadowMaps(VkImageView imageView, VkSampler sampler);  // Binding 8
    void BindEVSMShadows(VkImageView imageView, VkSampler sampler);  // Binding 9
    void BindRawDepthShadowMap(VkImageView imageView, VkSampler sampler);  // Binding 10 (non-comparison for raw depth access)

    // Bindless texture registration
    // Returns descriptor index for use in shaders
    u32 RegisterTexture(VkImageView imageView, VkSampler sampler);

    // Free a texture descriptor index for reuse
    void UnregisterTexture(u32 descriptorIndex);

    // Bind material SSBO (binding 0 in Set 1)
    void BindMaterialBuffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);

    // Get descriptor set layouts
    VkDescriptorSetLayout GetTransientLayout() const { return m_TransientLayout; }
    VkDescriptorSetLayout GetPersistentLayout() const { return m_PersistentLayout; }

    // Get descriptor sets
    VkDescriptorSet GetTransientSet(u32 frame) const { return m_TransientSets[frame]; }
    VkDescriptorSet GetPersistentSet() const { return m_PersistentSet; }

    // Legacy getters for compatibility (returns transient layout/set)
    VkDescriptorSetLayout GetLayout() const { return m_TransientLayout; }
    VkDescriptorSet GetDescriptorSet(u32 frame) const { return m_TransientSets[frame]; }

private:
    VulkanContext* m_Context = nullptr;
    u32 m_FramesInFlight = 0;

    // Set 0: Transient (per-frame camera UBO)
    VkDescriptorSetLayout m_TransientLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_TransientSets;  // One per frame

    // Set 1: Persistent (material SSBO + bindless textures)
    VkDescriptorSetLayout m_PersistentLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_PersistentSet = VK_NULL_HANDLE;

    // Per-frame uniform buffers (camera/view matrices)
    std::vector<VulkanBuffer> m_UniformBuffers;

    // Per-frame lighting buffers
    std::vector<VulkanBuffer> m_LightingBuffers;

    // Per-frame shadow buffers
    std::vector<VulkanBuffer> m_ShadowBuffers;

    // Bindless texture index management
    u32 m_NextTextureIndex = 0;
    std::queue<u32> m_FreeTextureIndices;

    void CreateDescriptorSetLayouts();
    void CreateDescriptorSets(u32 framesInFlight);
};

