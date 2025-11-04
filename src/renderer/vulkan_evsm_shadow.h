#pragma once

#include "core/types.h"
#include <vulkan/vulkan.h>
#include <mutex>

class VulkanContext;

// Manages EVSM (Exponential Variance Shadow Maps) moment generation
// Converts standard depth shadow maps to EVSM moment textures using compute shaders
class VulkanEVSMShadow {
public:
    struct Params {
        VkImage depthImage = VK_NULL_HANDLE;     // Input: standard depth shadow map
        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
        u32 width = 0;
        u32 height = 0;
        u32 layerCount = 1;                       // Number of array layers (cascades)
        f32 positiveExponent = 40.0f;
        f32 negativeExponent = 40.0f;
    };

    VulkanEVSMShadow() = default;
    ~VulkanEVSMShadow();

    void Initialize(VulkanContext* context, u32 resolution, u32 layerCount = 4);
    void Shutdown();
    bool IsInitialized() const { return m_Context != nullptr; }

    // Generate EVSM moments from depth shadow map
    void GenerateMoments(const Params& params);

    // Accessors for moment texture
    VkImage GetMomentsImage() const { return m_MomentsImage; }
    VkImageView GetMomentsImageView() const { return m_MomentsImageView; }
    VkSampler GetSampler() const { return m_Sampler; }

private:
    VulkanContext* m_Context = nullptr;
    u32 m_Resolution = 0;
    u32 m_LayerCount = 0;

    // EVSM moments texture (RGBA32F - RG = positive moments, BA = negative moments)
    VkImage m_MomentsImage = VK_NULL_HANDLE;
    VkDeviceMemory m_MomentsMemory = VK_NULL_HANDLE;
    VkImageView m_MomentsImageView = VK_NULL_HANDLE;
    VkSampler m_Sampler = VK_NULL_HANDLE;

    // Intermediate texture for ping-pong blur
    VkImage m_BlurTempImage = VK_NULL_HANDLE;
    VkDeviceMemory m_BlurTempMemory = VK_NULL_HANDLE;
    VkImageView m_BlurTempImageView = VK_NULL_HANDLE;

    // Compute pipeline for moment generation
    VkShaderModule m_ComputeShader = VK_NULL_HANDLE;
    VkPipeline m_ComputePipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

    // Blur compute pipeline
    VkShaderModule m_BlurShader = VK_NULL_HANDLE;
    VkPipeline m_BlurPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_BlurPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_BlurDescriptorSetLayout = VK_NULL_HANDLE;

    // Thread safety for descriptor pool and queue operations
    mutable std::mutex m_Mutex;

    void CreateMomentsImage();
    void CreateBlurTempImage();
    void CreateSampler();
    void CreateDescriptorSetLayout();
    void CreatePipelineLayout();
    void CreateDescriptorPool();
    void CreateComputePipeline();
    void CreateBlurDescriptorSetLayout();
    void CreateBlurPipelineLayout();
    void CreateBlurPipeline();

    VkDescriptorSet AllocateDescriptorSet();
    VkDescriptorSet AllocateBlurDescriptorSet();
    void UpdateDescriptorSet(VkDescriptorSet descriptorSet, VkImageView inputView, VkSampler inputSampler,
                             VkImageView outputView, u32 layer);
    void UpdateBlurDescriptorSet(VkDescriptorSet descriptorSet, VkImageView inputView, VkImageView outputView);
    void ApplyGaussianBlur(VkCommandBuffer commandBuffer, u32 layer);

    VkImageView CreateDepthImageView(VkImage image, VkFormat format, u32 layer) const;
    VkImageView CreateMomentsLayerView(u32 layer) const;
    VkImageView CreateBlurTempLayerView(u32 layer) const;

    u32 FindMemoryType(u32 typeFilter, VkMemoryPropertyFlags properties) const;
};
