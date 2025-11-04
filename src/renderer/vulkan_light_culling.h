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
              u32 framesInFlight, const LightCullingConfig& config = {});
    void Destroy();

    void Resize(u32 newWidth, u32 newHeight);

    // Update depth buffer descriptor for the given frame
    void UpdateDepthBuffer(u32 frameIndex, VkImageView depthBuffer);

    // Perform light culling
    void CullLights(VkCommandBuffer cmd, u32 frameIndex,
                    const Mat4& invProjection, const Mat4& viewMatrix,
                    u32 numLights);

    // Bind tile data for fragment shader
    void BindTileLightData(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout, u32 set);

    // Get last measured culling time in milliseconds
    f32 GetLastCullingTimeMs() const { return m_LastCullingTimeMs; }

    // Upload light data to SSBO
    void UploadLightData(const std::vector<GPULightForwardPlus>& lights);

    VkDescriptorSet GetDescriptorSet() const { return m_DescriptorSets[0]; }
    VkDescriptorSet GetComputeDescriptorSet(u32 frameIndex) const { return m_ComputeDescriptorSets[frameIndex]; }
    VkDescriptorSetLayout GetDescriptorLayout() const { return m_DescriptorLayout; }

    VkBuffer GetLightBuffer() const { return m_LightBuffer; }
    VkBuffer GetTileLightIndexBuffer() const { return m_TileLightIndexBuffer; }

private:
    void CreateBuffers();
    void CreateComputePipeline();
    void CreateDescriptorSets();
    void CreateTimestampQueries();
    void DestroyBuffers();
    void DestroyComputePipeline();
    void DestroyDescriptorSets();
    void DestroyTimestampQueries();
    void UpdateLightBufferDescriptors();
    void UpdateTimestampResults();

    VulkanContext* m_Context = nullptr;
    LightCullingConfig m_Config;

    u32 m_ScreenWidth = 0;
    u32 m_ScreenHeight = 0;
    u32 m_NumTilesX = 0;
    u32 m_NumTilesY = 0;
    u32 m_FramesInFlight = 0;

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
    std::vector<VkDescriptorSet> m_ComputeDescriptorSets;  // One per frame in flight
    VkDescriptorPool m_ComputeDescriptorPool = VK_NULL_HANDLE;

    // Fragment shader descriptor sets (for accessing tile data)
    VkDescriptorSetLayout m_DescriptorLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_DescriptorSets;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

    // Sampler for depth buffer
    VkSampler m_DepthSampler = VK_NULL_HANDLE;

    // Timestamp queries for performance measurement
    VkQueryPool m_TimestampQueryPool = VK_NULL_HANDLE;
    f32 m_LastCullingTimeMs = 0.0f;
    f32 m_TimestampPeriod = 1.0f;  // Nanoseconds per timestamp tick
};
