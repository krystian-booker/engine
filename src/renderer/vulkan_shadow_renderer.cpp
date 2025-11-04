#include "vulkan_shadow_renderer.h"
#include "vulkan_context.h"
#include "shadow_profiler.h"
#include "ecs/ecs_coordinator.h"
#include "ecs/systems/shadow_system.h"
#include "ecs/systems/render_system.h"
#include "ecs/components/light.h"
#include "ecs/components/transform.h"
#include "ecs/components/renderable.h"
#include "ecs/components/mesh_renderer.h"
#include "renderer/vertex.h"
#include "renderer/uniform_buffers.h"
#include "resources/mesh_manager.h"
#include "core/math.h"

#include <stdexcept>
#include <fstream>
#include <iostream>
#include <array>

namespace {

// Push constants for shadow rendering
struct ShadowPushConstants {
    Mat4 lightViewProj;
    Mat4 model;
};

std::vector<char> ReadFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));

    file.close();
    return buffer;
}

VkShaderModule CreateShaderModule(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const u32*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }

    return shaderModule;
}

} // anonymous namespace

VulkanShadowRenderer::~VulkanShadowRenderer() {
    Shutdown();
}

void VulkanShadowRenderer::Init(VulkanContext* context, ECSCoordinator* ecs) {
    if (!context || !ecs) {
        throw std::invalid_argument("VulkanShadowRenderer::Init requires valid context and ECS");
    }

    Shutdown();

    m_Context = context;
    m_ECS = ecs;

    // Create directional light cascaded shadow map
    m_DirectionalShadowMap = std::make_unique<VulkanShadowMap>();
    m_DirectionalShadowMap->CreateCascaded(m_Context, m_ShadowResolution, m_NumCascades);

    std::cout << "Created directional shadow map: " << m_ShadowResolution << "x" << m_ShadowResolution
              << ", " << m_NumCascades << " cascades" << std::endl;

    // Create shadow rendering pipeline
    CreateShadowPipeline();

    // Initialize profiler
    m_Profiler = std::make_unique<ShadowProfiler>();
    m_Profiler->Init(m_Context, 2);  // Assuming 2 frames in flight

    std::cout << "VulkanShadowRenderer initialized successfully" << std::endl;
}

void VulkanShadowRenderer::Shutdown() {
    if (!m_Context) {
        return;
    }

    VkDevice device = m_Context->GetDevice();
    vkDeviceWaitIdle(device);

    DestroyShadowPipeline();

    if (m_Profiler) {
        m_Profiler->Shutdown();
        m_Profiler.reset();
    }

    if (m_DirectionalShadowMap) {
        m_DirectionalShadowMap->Destroy();
        m_DirectionalShadowMap.reset();
    }

    if (m_PointSpotAtlas) {
        m_PointSpotAtlas->Destroy();
        m_PointSpotAtlas.reset();
    }

    m_Context = nullptr;
    m_ECS = nullptr;
}

void VulkanShadowRenderer::CreateShadowPipeline() {
    VkDevice device = m_Context->GetDevice();

    // Load shadow vertex shader
    auto vertShaderCode = ReadFile("assets/shaders/shadow.vert.spv");
    VkShaderModule vertShaderModule = CreateShaderModule(device, vertShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    // Vertex input
    auto bindingDescription = Vertex::GetBindingDescription();
    auto attributeDescriptions = Vertex::GetAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = 1;  // Only position needed
    vertexInputInfo.pVertexAttributeDescriptions = &attributeDescriptions[0];  // Position only

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor (dynamic)
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;  // Front-face culling for shadow maps
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_TRUE;  // Enable depth bias for shadow acne reduction
    rasterizer.depthBiasConstantFactor = 1.25f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 1.75f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // No color attachments for shadow pass
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 0;
    colorBlending.pAttachments = nullptr;

    // Push constants
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(ShadowPushConstants);

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;  // No descriptor sets needed
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_ShadowPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow pipeline layout");
    }

    // Dynamic state
    std::array<VkDynamicState, 2> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<u32>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // Create pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 1;  // Only vertex shader
    pipelineInfo.pStages = &vertShaderStageInfo;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_ShadowPipelineLayout;
    pipelineInfo.renderPass = m_DirectionalShadowMap->GetRenderPass();
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_ShadowPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow graphics pipeline");
    }

    vkDestroyShaderModule(device, vertShaderModule, nullptr);

    std::cout << "Shadow rendering pipeline created successfully" << std::endl;
}

void VulkanShadowRenderer::DestroyShadowPipeline() {
    if (!m_Context) {
        return;
    }

    VkDevice device = m_Context->GetDevice();

    if (m_ShadowPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_ShadowPipeline, nullptr);
        m_ShadowPipeline = VK_NULL_HANDLE;
    }

    if (m_ShadowPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_ShadowPipelineLayout, nullptr);
        m_ShadowPipelineLayout = VK_NULL_HANDLE;
    }

    if (m_ShadowDescriptorLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_ShadowDescriptorLayout, nullptr);
        m_ShadowDescriptorLayout = VK_NULL_HANDLE;
    }
}

void VulkanShadowRenderer::RenderShadows(VkCommandBuffer cmd, u32 frameIndex) {
    if (!HasShadowCastingLights()) {
        return;
    }

    // Begin profiling
    if (m_Profiler) {
        m_Profiler->BeginPass(cmd, frameIndex, "TotalShadowPass");
    }

    // Render directional light shadows
    RenderDirectionalShadows(cmd, frameIndex);

    // End profiling
    if (m_Profiler) {
        m_Profiler->EndPass(cmd, frameIndex, "TotalShadowPass");
    }

    // TODO: Render point/spot light shadows using atlas
}

void VulkanShadowRenderer::RenderDirectionalShadows(VkCommandBuffer cmd, u32 frameIndex) {
    if (!m_ShadowSystem) {
        return;  // Shadow system not set yet
    }

    const ShadowUniforms& shadowUniforms = m_ShadowSystem->GetShadowUniforms();
    u32 numCascades = static_cast<u32>(shadowUniforms.cascadeSplits.w);

    // Begin profiling directional shadows
    if (m_Profiler) {
        m_Profiler->BeginPass(cmd, frameIndex, "DirectionalShadows");
    }

    // Render each cascade
    for (u32 i = 0; i < numCascades && i < m_NumCascades; ++i) {
        RenderCascade(cmd, i, shadowUniforms.cascadeViewProj[i]);
    }

    // End profiling directional shadows
    if (m_Profiler) {
        m_Profiler->EndPass(cmd, frameIndex, "DirectionalShadows");
    }
}

void VulkanShadowRenderer::RenderCascade(VkCommandBuffer cmd, u32 cascadeIndex, const Mat4& lightViewProj) {
    // Begin render pass for this cascade
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_DirectionalShadowMap->GetRenderPass();
    renderPassInfo.framebuffer = m_DirectionalShadowMap->GetFramebuffer(cascadeIndex);
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent.width = m_ShadowResolution;
    renderPassInfo.renderArea.extent.height = m_ShadowResolution;

    VkClearValue clearValue{};
    clearValue.depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Bind shadow pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ShadowPipeline);

    // Set viewport and scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<f32>(m_ShadowResolution);
    viewport.height = static_cast<f32>(m_ShadowResolution);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent.width = m_ShadowResolution;
    scissor.extent.height = m_ShadowResolution;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Render all mesh renderers that cast shadows
    MeshManager& meshManager = MeshManager::Instance();

    m_ECS->ForEach<Transform, MeshRenderer>([&](Entity, Transform& transform, MeshRenderer& meshRenderer) {
        // Skip if this entity doesn't cast shadows
        if (!meshRenderer.castsShadows || !meshRenderer.visible) {
            return;
        }

        // Get mesh data
        MeshHandle meshHandle = meshManager.Load(meshRenderer.meshPath);
        if (!meshHandle.IsValid()) {
            return;
        }

        MeshData* meshData = meshManager.Get(meshHandle);
        if (!meshData) {
            return;
        }

        // Skip multi-mesh files (these should be loaded as individual sub-meshes)
        if (meshData->HasSubMeshes()) {
            return;
        }

        // Ensure mesh is uploaded to GPU
        if (!meshData->gpuUploaded) {
            meshData->gpuMesh.Create(m_Context, meshData);
            meshData->gpuUploaded = true;
        }

        // Skip if GPU mesh is not valid
        if (!meshData->gpuMesh.IsValid()) {
            return;
        }

        // Set push constants for this object
        ShadowPushConstants pushConstants{};
        pushConstants.lightViewProj = lightViewProj;
        pushConstants.model = transform.worldMatrix;

        vkCmdPushConstants(cmd, m_ShadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                          0, sizeof(ShadowPushConstants), &pushConstants);

        // Bind and draw mesh
        meshData->gpuMesh.Bind(cmd);
        meshData->gpuMesh.Draw(cmd);
    });

    vkCmdEndRenderPass(cmd);
}

bool VulkanShadowRenderer::HasShadowCastingLights() const {
    if (!m_ECS) {
        return false;
    }

    bool hasLights = false;
    m_ECS->ForEach<Light>([&](Entity, const Light& light) {
        if (light.castsShadows) {
            hasLights = true;
        }
    });

    return hasLights;
}

VkImage VulkanShadowRenderer::GetDirectionalShadowDepthImage() const {
    if (m_DirectionalShadowMap && m_DirectionalShadowMap->IsValid()) {
        return m_DirectionalShadowMap->GetDepthImage();
    }
    return VK_NULL_HANDLE;
}

VkImageView VulkanShadowRenderer::GetDirectionalShadowImageView() const {
    if (m_DirectionalShadowMap && m_DirectionalShadowMap->IsValid()) {
        return m_DirectionalShadowMap->GetDepthImageView();
    }
    return VK_NULL_HANDLE;
}

VkSampler VulkanShadowRenderer::GetDirectionalShadowSampler() const {
    if (m_DirectionalShadowMap && m_DirectionalShadowMap->IsValid()) {
        return m_DirectionalShadowMap->GetSampler();
    }
    return VK_NULL_HANDLE;
}

VkSampler VulkanShadowRenderer::GetDirectionalRawDepthSampler() const {
    if (m_DirectionalShadowMap && m_DirectionalShadowMap->IsValid()) {
        return m_DirectionalShadowMap->GetRawDepthSampler();
    }
    return VK_NULL_HANDLE;
}

VkFormat VulkanShadowRenderer::GetShadowFormat() const {
    if (m_DirectionalShadowMap && m_DirectionalShadowMap->IsValid()) {
        return m_DirectionalShadowMap->GetDepthFormat();
    }
    return VK_FORMAT_D32_SFLOAT;
}

u32 VulkanShadowRenderer::GetDirectionalShadowResolution() const {
    return m_ShadowResolution;
}

u32 VulkanShadowRenderer::GetNumCascades() const {
    return m_NumCascades;
}
