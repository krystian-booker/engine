#pragma once

#include "core/types.h"
#include "core/math.h"
#include "renderer/uniform_buffers.h"
#include <vulkan/vulkan.h>
#include <vector>

class VulkanContext;

struct LightCullingConfig {
    u32 tileSize = 16;
    u32 maxLightsPerTile = 256;
};

// Culling parameters UBO
struct CullingParams {
    Mat4 invProjection;
    Mat4 viewMatrix;
    Vec2 screenSize;
    u32 numLights;
    u32 padding;
};

class VulkanLightCulling {
public:
    void Init(VulkanContext* context, u32 screenWidth, u32 screenHeight,
              const LightCullingConfig& config = {});
    void Destroy();

    void Resize(u32 newWidth, u32 newHeight);

    // Perform light culling
    void CullLights(VkCommandBuffer cmd, VkImageView depthBuffer,
                    const Mat4& invProjection, const Mat4& viewMatrix,
                    u32 numLights);

    // Bind tile data for fragment shader
    void BindTileLightData(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout, u32 set);

    // Upload light data to SSBO
    void UploadLightData(const std::vector<GPULightForwardPlus>& lights);

    VkDescriptorSet GetDescriptorSet() const { return m_DescriptorSets[0]; }
    VkDescriptorSet GetComputeDescriptorSet() const { return m_ComputeDescriptorSet; }
    VkDescriptorSetLayout GetDescriptorLayout() const { return m_DescriptorLayout; }

    VkBuffer GetLightBuffer() const { return m_LightBuffer; }
    VkBuffer GetTileLightIndexBuffer() const { return m_TileLightIndexBuffer; }

private:
    void CreateBuffers();
    void CreateComputePipeline();
    void CreateDescriptorSets();
    void DestroyBuffers();
    void DestroyComputePipeline();
    void DestroyDescriptorSets();
    void UpdateLightBufferDescriptors();

    VulkanContext* m_Context = nullptr;
    LightCullingConfig m_Config;

    u32 m_ScreenWidth = 0;
    u32 m_ScreenHeight = 0;
    u32 m_NumTilesX = 0;
    u32 m_NumTilesY = 0;

    // Buffers
    VkBuffer m_LightBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_LightBufferMemory = VK_NULL_HANDLE;
    void* m_LightBufferMapped = nullptr;
    VkDeviceSize m_LightBufferSize = 0;

    VkBuffer m_TileLightIndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_TileLightIndexMemory = VK_NULL_HANDLE;

    VkBuffer m_CullingParamsBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_CullingParamsMemory = VK_NULL_HANDLE;
    void* m_CullingParamsMapped = nullptr;

    // Compute pipeline
    VkPipeline m_ComputePipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_ComputePipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_ComputeDescriptorLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_ComputeDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorPool m_ComputeDescriptorPool = VK_NULL_HANDLE;

    // Fragment shader descriptor sets (for accessing tile data)
    VkDescriptorSetLayout m_DescriptorLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_DescriptorSets;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

    // Sampler for depth buffer
    VkSampler m_DepthSampler = VK_NULL_HANDLE;
};
