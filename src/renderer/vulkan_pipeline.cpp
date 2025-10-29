#include "renderer/vulkan_pipeline.h"

#include "renderer/vulkan_context.h"
#include "renderer/vulkan_render_pass.h"
#include "renderer/vulkan_swapchain.h"
#include "renderer/vertex.h"
#include "renderer/push_constants.h"
#include "core/math.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <iostream>

namespace {
std::vector<std::filesystem::path> BuildShaderSearchPaths(const std::string& filename) {
    namespace fs = std::filesystem;

    const fs::path relativePath = fs::path(filename);
    std::vector<fs::path> candidates;

    auto addCandidate = [&candidates](const fs::path& path) {
        if (path.empty()) {
            return;
        }

        if (std::find(candidates.begin(), candidates.end(), path) == candidates.end()) {
            candidates.push_back(path);
        }
    };

    addCandidate(relativePath);

    fs::path current = fs::current_path();
    for (int i = 0; i < 3; ++i) {
        addCandidate(current / relativePath);
        if (!current.has_parent_path()) {
            break;
        }
        current = current.parent_path();
    }

#ifdef ENGINE_SOURCE_DIR
    addCandidate(fs::path(ENGINE_SOURCE_DIR) / relativePath);
#endif

    return candidates;
}
} // namespace

VulkanPipeline::~VulkanPipeline() {
    Shutdown();
}

void VulkanPipeline::Init(VulkanContext* context, VulkanRenderPass* renderPass, VulkanSwapchain* swapchain, VkDescriptorSetLayout descriptorSetLayout) {
    if (!context || !renderPass || !swapchain) {
        throw std::invalid_argument("VulkanPipeline::Init requires valid context, render pass, and swapchain");
    }

    if (descriptorSetLayout == VK_NULL_HANDLE) {
        throw std::invalid_argument("VulkanPipeline::Init requires a valid descriptor set layout");
    }

    Shutdown();

    m_Context = context;
    m_RenderPass = renderPass;

    // Create shared pipeline layout
    // Push constants defined in push_constants.h
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(m_Context->GetDevice(), &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan pipeline layout");
    }

    // Create all pipeline variants
    VkExtent2D extent = swapchain->GetExtent();

    for (u32 i = 0; i < static_cast<u32>(PipelineVariant::Count); ++i) {
        PipelineVariant variant = static_cast<PipelineVariant>(i);
        VkPipeline pipeline = CreatePipelineVariant(variant, descriptorSetLayout, extent);
        m_PipelineVariants[variant] = pipeline;
    }

    std::cout << "VulkanPipeline created " << static_cast<u32>(PipelineVariant::Count) << " pipeline variants" << std::endl;
}

void VulkanPipeline::Shutdown() {
    if (!m_Context) {
        m_PipelineVariants.clear();
        m_PipelineLayout = VK_NULL_HANDLE;
        return;
    }

    VkDevice device = m_Context->GetDevice();

    // Destroy all pipeline variants
    for (auto& [variant, pipeline] : m_PipelineVariants) {
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, pipeline, nullptr);
        }
    }
    m_PipelineVariants.clear();

    if (m_PipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
        m_PipelineLayout = VK_NULL_HANDLE;
    }

    m_Context = nullptr;
    m_RenderPass = nullptr;
}

VkPipeline VulkanPipeline::GetPipeline(PipelineVariant variant) const {
    auto it = m_PipelineVariants.find(variant);
    if (it != m_PipelineVariants.end()) {
        return it->second;
    }
    return VK_NULL_HANDLE;
}

VkPipeline VulkanPipeline::CreatePipelineVariant(PipelineVariant variant, VkDescriptorSetLayout /*descriptorSetLayout*/, VkExtent2D extent) {
    const auto vertShaderCode = ReadFile("assets/shaders/cube.vert.spv");
    const auto fragShaderCode = ReadFile("assets/shaders/cube.frag.spv");

    VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    const auto bindingDescription = Vertex::GetBindingDescription();
    const auto attributeDescriptions = Vertex::GetAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<u32>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<f32>(extent.width);
    viewport.height = static_cast<f32>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = extent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // Determine cull mode based on variant
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
    switch (variant) {
        case PipelineVariant::OpaqueDoubleSided:
        case PipelineVariant::AlphaBlendDoubleSided:
        case PipelineVariant::AlphaMaskDoubleSided:
            cullMode = VK_CULL_MODE_NONE;
            break;
        default:
            cullMode = VK_CULL_MODE_BACK_BIT;
            break;
    }

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = cullMode;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Determine blend state based on variant
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    switch (variant) {
        case PipelineVariant::AlphaBlend:
        case PipelineVariant::AlphaBlendDoubleSided:
            // Standard alpha blending: src*srcAlpha + dst*(1-srcAlpha)
            colorBlendAttachment.blendEnable = VK_TRUE;
            colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
            break;

        default:
            // Opaque and alpha-masked: no blending
            colorBlendAttachment.blendEnable = VK_FALSE;
            break;
    }

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Depth stencil state (alpha-blended objects should not write depth)
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    switch (variant) {
        case PipelineVariant::AlphaBlend:
        case PipelineVariant::AlphaBlendDoubleSided:
            // Alpha-blended objects should not write to depth buffer
            depthStencil.depthWriteEnable = VK_FALSE;
            break;

        default:
            // Opaque and alpha-masked objects write to depth
            depthStencil.depthWriteEnable = VK_TRUE;
            break;
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = ARRAY_COUNT(shaderStages);
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.layout = m_PipelineLayout;
    pipelineInfo.renderPass = m_RenderPass->Get();
    pipelineInfo.subpass = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(m_Context->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        vkDestroyShaderModule(m_Context->GetDevice(), vertShaderModule, nullptr);
        vkDestroyShaderModule(m_Context->GetDevice(), fragShaderModule, nullptr);
        throw std::runtime_error("Failed to create Vulkan graphics pipeline variant");
    }

    vkDestroyShaderModule(m_Context->GetDevice(), vertShaderModule, nullptr);
    vkDestroyShaderModule(m_Context->GetDevice(), fragShaderModule, nullptr);

    return pipeline;
}

VkShaderModule VulkanPipeline::CreateShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const u32*>(code.data());

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    if (vkCreateShaderModule(m_Context->GetDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan shader module");
    }

    return shaderModule;
}

std::vector<char> VulkanPipeline::ReadFile(const std::string& filename) {
    const auto searchPaths = BuildShaderSearchPaths(filename);

    for (const auto& path : searchPaths) {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            continue;
        }

        const size_t fileSize = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        return buffer;
    }

    throw std::runtime_error("Failed to open shader file: " + filename);
}
