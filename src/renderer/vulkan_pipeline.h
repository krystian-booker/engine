#pragma once

#include "core/types.h"
#include "renderer/pipeline_variant.h"

#include <vulkan/vulkan.h>

#include <string>
#include <vector>
#include <unordered_map>

class VulkanContext;
class VulkanRenderPass;
class VulkanSwapchain;

class VulkanPipeline {
public:
    VulkanPipeline() = default;
    ~VulkanPipeline();

    void Init(VulkanContext* context, VulkanRenderPass* renderPass, VulkanSwapchain* swapchain, VkDescriptorSetLayout descriptorSetLayout);
    void Shutdown();

    // Get pipeline for specific variant
    VkPipeline GetPipeline(PipelineVariant variant) const;

    // Get default opaque pipeline
    VkPipeline GetPipeline() const { return GetPipeline(PipelineVariant::Opaque); }

    VkPipelineLayout GetLayout() const { return m_PipelineLayout; }

private:
    VkShaderModule CreateShaderModule(const std::vector<char>& code);
    std::vector<char> ReadFile(const std::string& filename);

    // Create pipeline variant with specific blend/cull settings
    VkPipeline CreatePipelineVariant(PipelineVariant variant, VkDescriptorSetLayout descriptorSetLayout, VkExtent2D extent);

    VulkanContext* m_Context = nullptr;
    VulkanRenderPass* m_RenderPass = nullptr;

    // Pipeline layout (shared by all variants)
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;

    // Pipeline variants (different blend/cull states)
    std::unordered_map<PipelineVariant, VkPipeline> m_PipelineVariants;
};
