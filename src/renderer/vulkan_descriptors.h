#pragma once

#include "core/types.h"
#include "renderer/vulkan_buffer.h"

#include <vulkan/vulkan.h>

#include <vector>

class VulkanContext;

class VulkanDescriptors {
public:
    VulkanDescriptors() = default;
    ~VulkanDescriptors();

    void Init(VulkanContext* context, u32 framesInFlight);
    void Shutdown();

    void CreateUniformBuffers(u32 framesInFlight);
    void UpdateUniformBuffer(u32 currentFrame, const void* data, size_t size);

    VkDescriptorSetLayout GetLayout() const { return m_DescriptorSetLayout; }
    VkDescriptorSet GetDescriptorSet(u32 frame) const { return m_DescriptorSets[frame]; }

private:
    VulkanContext* m_Context = nullptr;

    VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_DescriptorSets;

    std::vector<VulkanBuffer> m_UniformBuffers;

    void CreateDescriptorSetLayout();
    void CreateDescriptorPool(u32 framesInFlight);
    void CreateDescriptorSets(u32 framesInFlight);
};

