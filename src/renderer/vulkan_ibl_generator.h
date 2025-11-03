#pragma once

#include "core/types.h"
#include <vulkan/vulkan.h>
#include <string>

class VulkanContext;
class VulkanTexture;

// IBL (Image-Based Lighting) map generator
// Converts HDR equirectangular environment maps to cubemaps and generates
// irradiance maps, prefiltered environment maps, and BRDF LUT for PBR rendering
class VulkanIBLGenerator {
public:
    VulkanIBLGenerator() = default;
    ~VulkanIBLGenerator();

    void Init(VulkanContext* context);
    void Shutdown();

    // Convert equirectangular HDR image to cubemap
    // Returns cubemap texture (ownership transferred to caller)
    VulkanTexture* ConvertEquirectangularToCubemap(const std::string& hdrPath, u32 resolution = 1024);

    // Generate irradiance cubemap for diffuse IBL (convolution)
    VulkanTexture* GenerateIrradianceMap(VulkanTexture* environmentCubemap, u32 resolution = 32);

    // Generate prefiltered environment map for specular IBL
    VulkanTexture* GeneratePrefilteredMap(VulkanTexture* environmentCubemap, u32 resolution = 128);

    // Generate BRDF integration lookup table (2D texture)
    VulkanTexture* GenerateBRDFLUT(u32 resolution = 512);

private:
    void CreateRenderPass();
    void CreateEquirectToCubePipeline();
    void CreateIrradiancePipeline();
    void CreatePrefilterPipeline();
    void CreateBRDFPipeline();

    VulkanContext* m_Context = nullptr;

    // Render passes
    VkRenderPass m_CubemapRenderPass = VK_NULL_HANDLE;
    VkRenderPass m_BRDFRenderPass = VK_NULL_HANDLE;

    // Pipelines
    VkPipeline m_EquirectToCubePipeline = VK_NULL_HANDLE;
    VkPipeline m_IrradiancePipeline = VK_NULL_HANDLE;
    VkPipeline m_PrefilterPipeline = VK_NULL_HANDLE;
    VkPipeline m_BRDFPipeline = VK_NULL_HANDLE;

    // Pipeline layouts
    VkPipelineLayout m_EquirectToCubeLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_IrradianceLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_PrefilterLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_BRDFLayout = VK_NULL_HANDLE;

    // Descriptor set layouts
    VkDescriptorSetLayout m_EquirectDescLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_ConvolutionDescLayout = VK_NULL_HANDLE;
};
