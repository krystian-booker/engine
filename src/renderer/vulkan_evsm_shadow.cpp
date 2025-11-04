#include "renderer/vulkan_evsm_shadow.h"
#include "renderer/vulkan_context.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace {

std::vector<char> ReadBinaryFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open shader file: " + path.string());
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(static_cast<size_t>(size));
    if (!file.read(buffer.data(), size)) {
        throw std::runtime_error("Failed to read shader file: " + path.string());
    }

    return buffer;
}

struct EVSMPushConstants {
    f32 positiveExponent;
    f32 negativeExponent;
    u32 mipLevel;
    u32 padding;
};
static_assert(sizeof(EVSMPushConstants) == 16, "Push constant size mismatch");

// RAII helper for command buffers
class ScopedCommandBuffer {
public:
    ScopedCommandBuffer(VkDevice device, VkCommandPool pool)
        : m_Device(device), m_Pool(pool) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = pool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(device, &allocInfo, &m_CommandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffer for EVSM compute");
        }
    }

    ~ScopedCommandBuffer() {
        if (m_CommandBuffer != VK_NULL_HANDLE) {
            vkFreeCommandBuffers(m_Device, m_Pool, 1, &m_CommandBuffer);
        }
    }

    ScopedCommandBuffer(const ScopedCommandBuffer&) = delete;
    ScopedCommandBuffer& operator=(const ScopedCommandBuffer&) = delete;

    VkCommandBuffer Get() const { return m_CommandBuffer; }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    VkCommandPool m_Pool = VK_NULL_HANDLE;
    VkCommandBuffer m_CommandBuffer = VK_NULL_HANDLE;
};

} // anonymous namespace

VulkanEVSMShadow::~VulkanEVSMShadow() {
    Shutdown();
}

void VulkanEVSMShadow::Initialize(VulkanContext* context, u32 resolution, u32 layerCount) {
    m_Context = context;
    m_Resolution = resolution;
    m_LayerCount = layerCount;

    CreateMomentsImage();
    CreateBlurTempImage();
    CreateSampler();
    CreateDescriptorSetLayout();
    CreatePipelineLayout();
    CreateDescriptorPool();
    CreateComputePipeline();
    CreateBlurDescriptorSetLayout();
    CreateBlurPipelineLayout();
    CreateBlurPipeline();
}

void VulkanEVSMShadow::Shutdown() {
    if (!m_Context) {
        return;
    }

    VkDevice device = m_Context->GetDevice();

    // Cleanup blur resources
    if (m_BlurPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_BlurPipeline, nullptr);
        m_BlurPipeline = VK_NULL_HANDLE;
    }

    if (m_BlurPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_BlurPipelineLayout, nullptr);
        m_BlurPipelineLayout = VK_NULL_HANDLE;
    }

    if (m_BlurShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, m_BlurShader, nullptr);
        m_BlurShader = VK_NULL_HANDLE;
    }

    if (m_BlurDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_BlurDescriptorSetLayout, nullptr);
        m_BlurDescriptorSetLayout = VK_NULL_HANDLE;
    }

    if (m_BlurTempImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_BlurTempImageView, nullptr);
        m_BlurTempImageView = VK_NULL_HANDLE;
    }

    if (m_BlurTempImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_BlurTempImage, nullptr);
        m_BlurTempImage = VK_NULL_HANDLE;
    }

    if (m_BlurTempMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_BlurTempMemory, nullptr);
        m_BlurTempMemory = VK_NULL_HANDLE;
    }

    // Cleanup moment generation resources
    if (m_ComputePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_ComputePipeline, nullptr);
        m_ComputePipeline = VK_NULL_HANDLE;
    }

    if (m_PipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
        m_PipelineLayout = VK_NULL_HANDLE;
    }

    if (m_ComputeShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, m_ComputeShader, nullptr);
        m_ComputeShader = VK_NULL_HANDLE;
    }

    if (m_DescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
        m_DescriptorPool = VK_NULL_HANDLE;
    }

    if (m_DescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_DescriptorSetLayout, nullptr);
        m_DescriptorSetLayout = VK_NULL_HANDLE;
    }

    if (m_Sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_Sampler, nullptr);
        m_Sampler = VK_NULL_HANDLE;
    }

    if (m_MomentsImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_MomentsImageView, nullptr);
        m_MomentsImageView = VK_NULL_HANDLE;
    }

    if (m_MomentsImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_MomentsImage, nullptr);
        m_MomentsImage = VK_NULL_HANDLE;
    }

    if (m_MomentsMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_MomentsMemory, nullptr);
        m_MomentsMemory = VK_NULL_HANDLE;
    }

    m_Context = nullptr;
}

void VulkanEVSMShadow::GenerateMoments(const Params& params) {
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (!m_Context || params.depthImage == VK_NULL_HANDLE) {
        return;
    }

    VkDevice device = m_Context->GetDevice();
    VkQueue computeQueue = m_Context->GetGraphicsQueue();  // Using graphics queue for compute
    VkCommandPool commandPool = m_Context->GetCommandPool();

    // Allocate command buffer
    ScopedCommandBuffer cmd(device, commandPool);
    VkCommandBuffer commandBuffer = cmd.Get();

    // Begin command buffer
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin EVSM compute command buffer");
    }

    // Process each layer
    for (u32 layer = 0; layer < params.layerCount; ++layer) {
        // Create descriptor set for this layer
        VkDescriptorSet descriptorSet = AllocateDescriptorSet();

        // Create image views for input and output
        VkImageView inputView = CreateDepthImageView(params.depthImage, params.depthFormat, layer);
        VkImageView outputView = CreateMomentsLayerView(layer);

        // Create sampler for depth map
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

        VkSampler depthSampler;
        if (vkCreateSampler(device, &samplerInfo, nullptr, &depthSampler) != VK_SUCCESS) {
            vkDestroyImageView(device, inputView, nullptr);
            vkDestroyImageView(device, outputView, nullptr);
            throw std::runtime_error("Failed to create depth sampler for EVSM");
        }

        // Update descriptor set
        UpdateDescriptorSet(descriptorSet, inputView, depthSampler, outputView, layer);

        // Transition moments image to general layout for compute write
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_MomentsImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = layer;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        // Bind pipeline and descriptor set
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ComputePipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_PipelineLayout,
                               0, 1, &descriptorSet, 0, nullptr);

        // Set push constants
        EVSMPushConstants pushConstants{};
        pushConstants.positiveExponent = params.positiveExponent;
        pushConstants.negativeExponent = params.negativeExponent;
        pushConstants.mipLevel = 0;
        pushConstants.padding = 0;

        vkCmdPushConstants(commandBuffer, m_PipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                          0, sizeof(EVSMPushConstants), &pushConstants);

        // Dispatch compute shader (8x8 workgroup size)
        u32 groupsX = (params.width + 7) / 8;
        u32 groupsY = (params.height + 7) / 8;
        vkCmdDispatch(commandBuffer, groupsX, groupsY, 1);

        // Barrier: Wait for moment generation to complete before blur
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        // Apply Gaussian blur to reduce noise and light bleeding
        ApplyGaussianBlur(commandBuffer, layer);

        // Transition to shader read layout for fragment shader
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        // Cleanup temporary resources
        vkDestroySampler(device, depthSampler, nullptr);
        vkDestroyImageView(device, outputView, nullptr);
        vkDestroyImageView(device, inputView, nullptr);
    }

    // End command buffer
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to end EVSM compute command buffer");
    }

    // Submit command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    // Create fence for synchronization
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence;
    if (vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create fence for EVSM compute");
    }

    if (vkQueueSubmit(computeQueue, 1, &submitInfo, fence) != VK_SUCCESS) {
        vkDestroyFence(device, fence, nullptr);
        throw std::runtime_error("Failed to submit EVSM compute command buffer");
    }

    // Wait for completion
    vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(device, fence, nullptr);
}

void VulkanEVSMShadow::CreateMomentsImage() {
    VkDevice device = m_Context->GetDevice();

    // Create RGBA32F image for moments storage
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    imageInfo.extent.width = m_Resolution;
    imageInfo.extent.height = m_Resolution;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = m_LayerCount;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &imageInfo, nullptr, &m_MomentsImage) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create EVSM moments image");
    }

    // Allocate memory
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, m_MomentsImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_MomentsMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate EVSM moments image memory");
    }

    vkBindImageMemory(device, m_MomentsImage, m_MomentsMemory, 0);

    // Create image view for entire array
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_MomentsImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = m_LayerCount;

    if (vkCreateImageView(device, &viewInfo, nullptr, &m_MomentsImageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create EVSM moments image view");
    }
}

void VulkanEVSMShadow::CreateSampler() {
    VkDevice device = m_Context->GetDevice();

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;  // Not a comparison sampler
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &m_Sampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create EVSM sampler");
    }
}

void VulkanEVSMShadow::CreateDescriptorSetLayout() {
    VkDevice device = m_Context->GetDevice();

    VkDescriptorSetLayoutBinding bindings[2] = {};

    // Binding 0: Input depth texture
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 1: Output moments storage image
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings = bindings;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create EVSM descriptor set layout");
    }
}

void VulkanEVSMShadow::CreatePipelineLayout() {
    VkDevice device = m_Context->GetDevice();

    // Push constants for exponents
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(EVSMPushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_DescriptorSetLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create EVSM pipeline layout");
    }
}

void VulkanEVSMShadow::CreateDescriptorPool() {
    VkDevice device = m_Context->GetDevice();

    VkDescriptorPoolSize poolSizes[2] = {};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 100;  // Support multiple dispatches
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = 100;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = 100;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create EVSM descriptor pool");
    }
}

void VulkanEVSMShadow::CreateComputePipeline() {
    VkDevice device = m_Context->GetDevice();

    // Load shader
    std::filesystem::path shaderPath = std::filesystem::path(ENGINE_SOURCE_DIR) / "assets" / "shaders" / "evsm_prefilter.comp.spv";
    std::vector<char> shaderCode = ReadBinaryFile(shaderPath);

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = shaderCode.size();
    createInfo.pCode = reinterpret_cast<const u32*>(shaderCode.data());

    if (vkCreateShaderModule(device, &createInfo, nullptr, &m_ComputeShader) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create EVSM compute shader module");
    }

    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfo.module = m_ComputeShader;
    shaderStageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = shaderStageInfo;
    pipelineInfo.layout = m_PipelineLayout;

    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_ComputePipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create EVSM compute pipeline");
    }
}

VkDescriptorSet VulkanEVSMShadow::AllocateDescriptorSet() {
    VkDevice device = m_Context->GetDevice();

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_DescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_DescriptorSetLayout;

    VkDescriptorSet descriptorSet;
    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate EVSM descriptor set");
    }

    return descriptorSet;
}

void VulkanEVSMShadow::UpdateDescriptorSet(VkDescriptorSet descriptorSet, VkImageView inputView,
                                           VkSampler inputSampler, VkImageView outputView, u32 layer) {
    (void)layer;  // Unused parameter, kept for future per-layer descriptor sets
    VkDevice device = m_Context->GetDevice();

    VkDescriptorImageInfo inputImageInfo{};
    inputImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    inputImageInfo.imageView = inputView;
    inputImageInfo.sampler = inputSampler;

    VkDescriptorImageInfo outputImageInfo{};
    outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    outputImageInfo.imageView = outputView;

    VkWriteDescriptorSet descriptorWrites[2] = {};

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = descriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pImageInfo = &inputImageInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = descriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &outputImageInfo;

    vkUpdateDescriptorSets(device, 2, descriptorWrites, 0, nullptr);
}

VkImageView VulkanEVSMShadow::CreateDepthImageView(VkImage image, VkFormat format, u32 layer) const {
    VkDevice device = m_Context->GetDevice();

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = layer;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create depth image view for EVSM");
    }

    return imageView;
}

VkImageView VulkanEVSMShadow::CreateMomentsLayerView(u32 layer) const {
    VkDevice device = m_Context->GetDevice();

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_MomentsImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = layer;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create moments layer view for EVSM");
    }

    return imageView;
}

u32 VulkanEVSMShadow::FindMemoryType(u32 typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDevice physicalDevice = m_Context->GetPhysicalDevice();

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (u32 i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type for EVSM");
}

void VulkanEVSMShadow::CreateBlurTempImage() {
    VkDevice device = m_Context->GetDevice();

    // Create temporary RGBA32F image for ping-pong blur
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    imageInfo.extent.width = m_Resolution;
    imageInfo.extent.height = m_Resolution;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = m_LayerCount;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &imageInfo, nullptr, &m_BlurTempImage) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create EVSM blur temp image");
    }

    // Allocate memory
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, m_BlurTempImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_BlurTempMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate EVSM blur temp image memory");
    }

    vkBindImageMemory(device, m_BlurTempImage, m_BlurTempMemory, 0);

    // Create image view for entire array
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_BlurTempImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = m_LayerCount;

    if (vkCreateImageView(device, &viewInfo, nullptr, &m_BlurTempImageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create EVSM blur temp image view");
    }
}

VkImageView VulkanEVSMShadow::CreateBlurTempLayerView(u32 layer) const {
    VkDevice device = m_Context->GetDevice();

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_BlurTempImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = layer;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create blur temp layer view for EVSM");
    }

    return imageView;
}

void VulkanEVSMShadow::CreateBlurDescriptorSetLayout() {
    VkDevice device = m_Context->GetDevice();

    VkDescriptorSetLayoutBinding bindings[2] = {};

    // Binding 0: Input moments texture
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 1: Output storage image
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings = bindings;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_BlurDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create EVSM blur descriptor set layout");
    }
}

void VulkanEVSMShadow::CreateBlurPipelineLayout() {
    VkDevice device = m_Context->GetDevice();

    // Push constants for blur parameters
    struct BlurPushConstants {
        u32 horizontal;
        f32 blurRadius;
        u32 padding1;
        u32 padding2;
    };

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(BlurPushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_BlurDescriptorSetLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_BlurPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create EVSM blur pipeline layout");
    }
}

void VulkanEVSMShadow::CreateBlurPipeline() {
    VkDevice device = m_Context->GetDevice();

    // Load blur shader
    std::filesystem::path shaderPath = std::filesystem::path(ENGINE_SOURCE_DIR) / "assets" / "shaders" / "evsm_blur.comp.spv";
    std::vector<char> shaderCode = ReadBinaryFile(shaderPath);

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = shaderCode.size();
    createInfo.pCode = reinterpret_cast<const u32*>(shaderCode.data());

    if (vkCreateShaderModule(device, &createInfo, nullptr, &m_BlurShader) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create EVSM blur shader module");
    }

    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfo.module = m_BlurShader;
    shaderStageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = shaderStageInfo;
    pipelineInfo.layout = m_BlurPipelineLayout;

    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_BlurPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create EVSM blur pipeline");
    }
}

VkDescriptorSet VulkanEVSMShadow::AllocateBlurDescriptorSet() {
    VkDevice device = m_Context->GetDevice();

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_DescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_BlurDescriptorSetLayout;

    VkDescriptorSet descriptorSet;
    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate EVSM blur descriptor set");
    }

    return descriptorSet;
}

void VulkanEVSMShadow::UpdateBlurDescriptorSet(VkDescriptorSet descriptorSet, VkImageView inputView, VkImageView outputView) {
    VkDevice device = m_Context->GetDevice();

    VkDescriptorImageInfo inputImageInfo{};
    inputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    inputImageInfo.imageView = inputView;
    inputImageInfo.sampler = m_Sampler;

    VkDescriptorImageInfo outputImageInfo{};
    outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    outputImageInfo.imageView = outputView;

    VkWriteDescriptorSet descriptorWrites[2] = {};

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = descriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pImageInfo = &inputImageInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = descriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &outputImageInfo;

    vkUpdateDescriptorSets(device, 2, descriptorWrites, 0, nullptr);
}

void VulkanEVSMShadow::ApplyGaussianBlur(VkCommandBuffer commandBuffer, u32 layer) {
    VkDevice device = m_Context->GetDevice();

    // Create layer views for ping-pong blur
    VkImageView momentsLayerView = CreateMomentsLayerView(layer);
    VkImageView blurTempLayerView = CreateBlurTempLayerView(layer);

    // Push constants for blur
    struct BlurPushConstants {
        u32 horizontal;
        f32 blurRadius;
        u32 padding1;
        u32 padding2;
    };

    // Pass 1: Horizontal blur (moments -> temp)
    {
        VkDescriptorSet descriptorSet = AllocateBlurDescriptorSet();
        UpdateBlurDescriptorSet(descriptorSet, momentsLayerView, blurTempLayerView);

        // Transition blur temp image to general layout
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_BlurTempImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = layer;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_BlurPipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_BlurPipelineLayout,
                               0, 1, &descriptorSet, 0, nullptr);

        BlurPushConstants pushConstants{};
        pushConstants.horizontal = 1;  // Horizontal pass
        pushConstants.blurRadius = 2.0f;
        pushConstants.padding1 = 0;
        pushConstants.padding2 = 0;

        vkCmdPushConstants(commandBuffer, m_BlurPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                          0, sizeof(BlurPushConstants), &pushConstants);

        u32 groupsX = (m_Resolution + 7) / 8;
        u32 groupsY = (m_Resolution + 7) / 8;
        vkCmdDispatch(commandBuffer, groupsX, groupsY, 1);

        // Barrier before second pass
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    // Pass 2: Vertical blur (temp -> moments)
    {
        VkDescriptorSet descriptorSet = AllocateBlurDescriptorSet();
        UpdateBlurDescriptorSet(descriptorSet, blurTempLayerView, momentsLayerView);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_BlurPipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_BlurPipelineLayout,
                               0, 1, &descriptorSet, 0, nullptr);

        BlurPushConstants pushConstants{};
        pushConstants.horizontal = 0;  // Vertical pass
        pushConstants.blurRadius = 2.0f;
        pushConstants.padding1 = 0;
        pushConstants.padding2 = 0;

        vkCmdPushConstants(commandBuffer, m_BlurPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                          0, sizeof(BlurPushConstants), &pushConstants);

        u32 groupsX = (m_Resolution + 7) / 8;
        u32 groupsY = (m_Resolution + 7) / 8;
        vkCmdDispatch(commandBuffer, groupsX, groupsY, 1);
    }

    // Cleanup temporary views
    vkDestroyImageView(device, blurTempLayerView, nullptr);
    vkDestroyImageView(device, momentsLayerView, nullptr);
}
