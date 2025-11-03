#pragma once

#include "core/types.h"
#include <vulkan/vulkan.h>
#include <vector>

class VulkanContext;
class VulkanTexture;

// Post-processing configuration
struct PostProcessConfig {
    // HDR & Tone Mapping
    enum class ToneMapper {
        None,
        Reinhard,
        ReinhardLuminance,
        Uncharted2,
        ACES,
        ACESFitted
    };
    ToneMapper toneMapper = ToneMapper::ACESFitted;
    f32 exposure = 1.0f;
    bool autoExposure = false;
    f32 autoExposureSpeed = 3.0f;
    f32 minExposure = 0.1f;
    f32 maxExposure = 10.0f;

    // Bloom
    bool enableBloom = true;
    f32 bloomThreshold = 1.0f;
    f32 bloomIntensity = 0.04f;
    u32 bloomIterations = 5;
    f32 bloomRadius = 1.0f;

    // SSAO
    bool enableSSAO = true;
    f32 ssaoRadius = 0.5f;
    f32 ssaoBias = 0.025f;
    f32 ssaoIntensity = 1.5f;
    u32 ssaoSamples = 16;
    u32 ssaoNoiseSize = 4;

    // Color Grading
    bool enableColorGrading = false;
    TextureHandle colorGradingLUT = {};  // 3D LUT texture handle

    // Vignette
    bool enableVignette = false;
    f32 vignetteIntensity = 0.3f;
    f32 vignetteRadius = 0.8f;
};

// Post-processing pipeline manager
// Handles HDR rendering, tone mapping, bloom, SSAO, and composition
class VulkanPostProcess {
public:
    VulkanPostProcess() = default;
    ~VulkanPostProcess();

    void Init(VulkanContext* context, u32 width, u32 height);
    void Shutdown();
    void Resize(u32 width, u32 height);

    // Process HDR scene texture and output to LDR target
    void Process(VkCommandBuffer cmd, VkImageView hdrInput, VkImageView depthInput,
                 VkImageView normalInput, VkImageView outputTarget);

    // Configuration
    void SetConfig(const PostProcessConfig& config) { m_Config = config; }
    const PostProcessConfig& GetConfig() const { return m_Config; }

    // Get intermediate textures for debugging
    VkImageView GetBloomTexture() const;
    VkImageView GetSSAOTexture() const;

private:
    void CreateRenderPasses();
    void CreatePipelines();
    void CreateFramebuffers();
    void CreateTextures();
    void CreateSamplers();
    void GenerateSSAOKernel();
    void GenerateSSAONoise();

    // Post-process stages
    void RenderBrightPass(VkCommandBuffer cmd, VkImageView hdrInput);
    void RenderBloomDownsample(VkCommandBuffer cmd);
    void RenderBloomUpsample(VkCommandBuffer cmd);
    void RenderSSAO(VkCommandBuffer cmd, VkImageView depthInput, VkImageView normalInput);
    void RenderComposite(VkCommandBuffer cmd, VkImageView hdrInput, VkImageView outputTarget);

    VulkanContext* m_Context = nullptr;
    u32 m_Width = 0;
    u32 m_Height = 0;
    PostProcessConfig m_Config;

    // Render passes
    VkRenderPass m_BrightPassRP = VK_NULL_HANDLE;
    VkRenderPass m_BloomRP = VK_NULL_HANDLE;
    VkRenderPass m_SSAORP = VK_NULL_HANDLE;
    VkRenderPass m_CompositeRP = VK_NULL_HANDLE;

    // Pipelines
    VkPipeline m_BrightPassPipeline = VK_NULL_HANDLE;
    VkPipeline m_BloomDownsamplePipeline = VK_NULL_HANDLE;
    VkPipeline m_BloomUpsamplePipeline = VK_NULL_HANDLE;
    VkPipeline m_SSAOPipeline = VK_NULL_HANDLE;
    VkPipeline m_SSAOBlurPipeline = VK_NULL_HANDLE;
    VkPipeline m_CompositePipeline = VK_NULL_HANDLE;

    // Pipeline layouts
    VkPipelineLayout m_BrightPassLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_BloomLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_SSAOLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_CompositeLayout = VK_NULL_HANDLE;

    // Descriptor set layouts
    VkDescriptorSetLayout m_BrightPassDescLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_BloomDescLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_SSAODescLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_CompositeDescLayout = VK_NULL_HANDLE;

    // Textures for bloom chain
    struct BloomMip {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        u32 width = 0;
        u32 height = 0;
    };
    std::vector<BloomMip> m_BloomMips;

    // SSAO textures
    VkImage m_SSAOImage = VK_NULL_HANDLE;
    VkDeviceMemory m_SSAOMemory = VK_NULL_HANDLE;
    VkImageView m_SSAOView = VK_NULL_HANDLE;
    VkFramebuffer m_SSAOFramebuffer = VK_NULL_HANDLE;

    VkImage m_SSAOBlurImage = VK_NULL_HANDLE;
    VkDeviceMemory m_SSAOBlurMemory = VK_NULL_HANDLE;
    VkImageView m_SSAOBlurView = VK_NULL_HANDLE;
    VkFramebuffer m_SSAOBlurFramebuffer = VK_NULL_HANDLE;

    // SSAO kernel and noise
    std::vector<f32> m_SSAOKernel;  // Vec3 samples in tangent space
    VkImage m_SSAONoiseImage = VK_NULL_HANDLE;
    VkDeviceMemory m_SSAONoiseMemory = VK_NULL_HANDLE;
    VkImageView m_SSAONoiseView = VK_NULL_HANDLE;

    // Samplers
    VkSampler m_LinearSampler = VK_NULL_HANDLE;
    VkSampler m_NearestSampler = VK_NULL_HANDLE;
    VkSampler m_SSAONoiseSampler = VK_NULL_HANDLE;

    // Average luminance for auto-exposure
    f32 m_CurrentExposure = 1.0f;
};
