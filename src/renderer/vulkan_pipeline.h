#pragma once

#include "core/types.h"

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

class VulkanContext;
class VulkanRenderPass;
class VulkanSwapchain;

class VulkanPipeline {
public:
    VulkanPipeline() = default;
    ~VulkanPipeline();

    void Init(VulkanContext* context, VulkanRenderPass* renderPass, VulkanSwapchain* swapchain, VkDescriptorSetLayout descriptorSetLayout);
    void Shutdown();

    VkPipeline GetPipeline() const { return m_Pipeline; }
    VkPipelineLayout GetLayout() const { return m_PipelineLayout; }

private:
    VkShaderModule CreateShaderModule(const std::vector<char>& code);
    std::vector<char> ReadFile(const std::string& filename);

    VulkanContext* m_Context = nullptr;
    VkPipeline m_Pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
};
