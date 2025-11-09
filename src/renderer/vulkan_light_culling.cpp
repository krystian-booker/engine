#include "renderer/vulkan_light_culling.h"

#include "renderer/vulkan_context.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>

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

u32 FindMemoryType(VkPhysicalDevice physicalDevice, u32 typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (u32 i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}

} // namespace

void VulkanLightCulling::Init(VulkanContext* context, u32 screenWidth, u32 screenHeight,
                               u32 framesInFlight, const LightCullingConfig& config) {
    m_Context = context;
    m_Config = config;
    m_ScreenWidth = screenWidth;
    m_ScreenHeight = screenHeight;
    m_FramesInFlight = framesInFlight;

    // Calculate number of tiles
    m_NumTilesX = (m_ScreenWidth + m_Config.tileSize - 1) / m_Config.tileSize;
    m_NumTilesY = (m_ScreenHeight + m_Config.tileSize - 1) / m_Config.tileSize;

    CreateBuffers();
    CreateDescriptorSets();
    CreateComputePipeline();
    CreateTimestampQueries();
}

void VulkanLightCulling::Destroy() {
    DestroyTimestampQueries();
    DestroyComputePipeline();
    DestroyDescriptorSets();
    DestroyBuffers();

    m_Context = nullptr;
}

void VulkanLightCulling::Resize(u32 newWidth, u32 newHeight) {
    if (newWidth == m_ScreenWidth && newHeight == m_ScreenHeight) {
        return;
    }

    m_ScreenWidth = newWidth;
    m_ScreenHeight = newHeight;

    // Recalculate tiles
    m_NumTilesX = (m_ScreenWidth + m_Config.tileSize - 1) / m_Config.tileSize;
    m_NumTilesY = (m_ScreenHeight + m_Config.tileSize - 1) / m_Config.tileSize;

    // Recreate tile buffer
    DestroyBuffers();
    CreateBuffers();

    // Update descriptor sets
    DestroyDescriptorSets();
    CreateDescriptorSets();
}

void VulkanLightCulling::UpdateDepthBuffer(u32 frameIndex, VkImageView depthBuffer) {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageView = depthBuffer;
    imageInfo.sampler = m_DepthSampler;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_ComputeDescriptorSets[frameIndex];
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_Context->GetDevice(), 1, &write, 0, nullptr);
}

void VulkanLightCulling::CullLights(VkCommandBuffer cmd, u32 frameIndex,
                                     const Mat4& invProjection, const Mat4& viewMatrix,
                                     u32 numLights) {
    // Update culling parameters
    CullingParams params{};
    params.invProjection = invProjection;
    params.viewMatrix = viewMatrix;
    params.screenSize = Vec2(static_cast<f32>(m_ScreenWidth), static_cast<f32>(m_ScreenHeight));
    params.numLights = numLights;
    params.padding = 0;

    memcpy(m_CullingParamsMapped, &params, sizeof(CullingParams));

    // Reset query pool and write start timestamp
    if (m_TimestampQueryPool != VK_NULL_HANDLE) {
        vkCmdResetQueryPool(cmd, m_TimestampQueryPool, frameIndex * 2, 2);
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_TimestampQueryPool, frameIndex * 2);
    }

    // Dispatch compute shader
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_ComputePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_ComputePipelineLayout,
                            0, 1, &m_ComputeDescriptorSets[frameIndex], 0, nullptr);

    u32 dispatchX = m_NumTilesX;
    u32 dispatchY = m_NumTilesY;
    vkCmdDispatch(cmd, dispatchX, dispatchY, 1);

    // Write end timestamp
    if (m_TimestampQueryPool != VK_NULL_HANDLE) {
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_TimestampQueryPool, frameIndex * 2 + 1);
    }

    // Memory barrier to ensure tile data is written before fragment shader reads it
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0,
                         1, &barrier,
                         0, nullptr,
                         0, nullptr);

    // Update timestamp results (reads from previous frames to avoid stalling)
    UpdateTimestampResults();
}

void VulkanLightCulling::BindTileLightData(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout, u32 set) {
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                            set, 1, &m_DescriptorSets[0], 0, nullptr);
}

void VulkanLightCulling::UploadLightData(const std::vector<GPULightForwardPlus>& lights) {
    if (lights.empty()) {
        return;
    }

    VkDeviceSize requiredSize = lights.size() * sizeof(GPULightForwardPlus);
    if (requiredSize > m_LightBufferSize) {
        // Recreate larger buffer
        if (m_LightBuffer != VK_NULL_HANDLE) {
            vkUnmapMemory(m_Context->GetDevice(), m_LightBufferMemory);
            vkDestroyBuffer(m_Context->GetDevice(), m_LightBuffer, nullptr);
            vkFreeMemory(m_Context->GetDevice(), m_LightBufferMemory, nullptr);
        }

        // Create larger buffer with some headroom
        m_LightBufferSize = requiredSize * 2;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = m_LightBufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(m_Context->GetDevice(), &bufferInfo, nullptr, &m_LightBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create light buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_Context->GetDevice(), m_LightBuffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(m_Context->GetPhysicalDevice(),
                                                    memRequirements.memoryTypeBits,
                                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(m_Context->GetDevice(), &allocInfo, nullptr, &m_LightBufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate light buffer memory!");
        }

        vkBindBufferMemory(m_Context->GetDevice(), m_LightBuffer, m_LightBufferMemory, 0);

        // Remap memory
        vkMapMemory(m_Context->GetDevice(), m_LightBufferMemory, 0, m_LightBufferSize, 0, &m_LightBufferMapped);

        // Update descriptor sets
        UpdateLightBufferDescriptors();
    }

    // Upload light data
    memcpy(m_LightBufferMapped, lights.data(), requiredSize);
}

void VulkanLightCulling::CreateBuffers() {
    VkDevice device = m_Context->GetDevice();

    // Create light buffer (initial size for 256 lights)
    m_LightBufferSize = 256 * sizeof(GPULightForwardPlus);

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = m_LightBufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &m_LightBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create light buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, m_LightBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(m_Context->GetPhysicalDevice(),
                                                memRequirements.memoryTypeBits,
                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_LightBufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate light buffer memory!");
    }

    vkBindBufferMemory(device, m_LightBuffer, m_LightBufferMemory, 0);
    vkMapMemory(device, m_LightBufferMemory, 0, m_LightBufferSize, 0, &m_LightBufferMapped);

    // Create tile light index buffer
    u32 numTiles = m_NumTilesX * m_NumTilesY;
    VkDeviceSize tileBufferSize = numTiles * (sizeof(u32) + m_Config.maxLightsPerTile * sizeof(u32));

    bufferInfo.size = tileBufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &m_TileLightIndexBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create tile light index buffer!");
    }

    vkGetBufferMemoryRequirements(device, m_TileLightIndexBuffer, &memRequirements);

    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(m_Context->GetPhysicalDevice(),
                                                memRequirements.memoryTypeBits,
                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_TileLightIndexMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate tile light index buffer memory!");
    }

    vkBindBufferMemory(device, m_TileLightIndexBuffer, m_TileLightIndexMemory, 0);

    // Create culling params buffer
    bufferInfo.size = sizeof(CullingParams);
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &m_CullingParamsBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create culling params buffer!");
    }

    vkGetBufferMemoryRequirements(device, m_CullingParamsBuffer, &memRequirements);

    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(m_Context->GetPhysicalDevice(),
                                                memRequirements.memoryTypeBits,
                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_CullingParamsMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate culling params buffer memory!");
    }

    vkBindBufferMemory(device, m_CullingParamsBuffer, m_CullingParamsMemory, 0);
    vkMapMemory(device, m_CullingParamsMemory, 0, sizeof(CullingParams), 0, &m_CullingParamsMapped);

    // Create depth sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &m_DepthSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create depth sampler!");
    }
}

void VulkanLightCulling::CreateComputePipeline() {
    VkDevice device = m_Context->GetDevice();

    // Create descriptor set layout for compute shader
    std::array<VkDescriptorSetLayoutBinding, 4> bindings{};

    // Binding 0: Depth buffer (sampler2D)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 1: Culling params (UBO)
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 2: Light buffer (SSBO)
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 3: Tile light index buffer (SSBO)
    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<u32>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_ComputeDescriptorLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute descriptor set layout!");
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_ComputeDescriptorLayout;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_ComputePipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute pipeline layout!");
    }

    // Load compute shader
    std::filesystem::path shaderPath = std::filesystem::path(ENGINE_SOURCE_DIR) / "assets" / "shaders" / "light_culling.comp.spv";
    auto shaderCode = ReadBinaryFile(shaderPath);

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = shaderCode.size();
    createInfo.pCode = reinterpret_cast<const u32*>(shaderCode.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute shader module!");
    }

    // Create compute pipeline
    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfo.module = shaderModule;
    shaderStageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = shaderStageInfo;
    pipelineInfo.layout = m_ComputePipelineLayout;

    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_ComputePipeline) != VK_SUCCESS) {
        vkDestroyShaderModule(device, shaderModule, nullptr);
        throw std::runtime_error("Failed to create compute pipeline!");
    }

    vkDestroyShaderModule(device, shaderModule, nullptr);

    // Create descriptor pool for compute (one set per frame in flight)
    std::array<VkDescriptorPoolSize, 3> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = m_FramesInFlight;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[1].descriptorCount = m_FramesInFlight;
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[2].descriptorCount = m_FramesInFlight * 2;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = m_FramesInFlight;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_ComputeDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute descriptor pool!");
    }

    // Allocate descriptor sets (one per frame in flight)
    m_ComputeDescriptorSets.resize(m_FramesInFlight);
    std::vector<VkDescriptorSetLayout> layouts(m_FramesInFlight, m_ComputeDescriptorLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_ComputeDescriptorPool;
    allocInfo.descriptorSetCount = m_FramesInFlight;
    allocInfo.pSetLayouts = layouts.data();

    if (vkAllocateDescriptorSets(device, &allocInfo, m_ComputeDescriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate compute descriptor sets!");
    }

    // Update descriptor sets for all frames (depth buffer will be updated later per-frame)
    for (u32 i = 0; i < m_FramesInFlight; ++i) {
        std::array<VkWriteDescriptorSet, 3> descriptorWrites{};

        VkDescriptorBufferInfo paramsBufferInfo{};
        paramsBufferInfo.buffer = m_CullingParamsBuffer;
        paramsBufferInfo.offset = 0;
        paramsBufferInfo.range = sizeof(CullingParams);

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = m_ComputeDescriptorSets[i];
        descriptorWrites[0].dstBinding = 1;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &paramsBufferInfo;

        VkDescriptorBufferInfo lightBufferInfo{};
        lightBufferInfo.buffer = m_LightBuffer;
        lightBufferInfo.offset = 0;
        lightBufferInfo.range = m_LightBufferSize;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = m_ComputeDescriptorSets[i];
        descriptorWrites[1].dstBinding = 2;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &lightBufferInfo;

        VkDescriptorBufferInfo tileBufferInfo{};
        tileBufferInfo.buffer = m_TileLightIndexBuffer;
        tileBufferInfo.offset = 0;
        tileBufferInfo.range = VK_WHOLE_SIZE;

        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = m_ComputeDescriptorSets[i];
        descriptorWrites[2].dstBinding = 3;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &tileBufferInfo;

        vkUpdateDescriptorSets(device, static_cast<u32>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
}

void VulkanLightCulling::CreateDescriptorSets() {
    VkDevice device = m_Context->GetDevice();

    // Create descriptor set layout for fragment shader access
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};

    // Binding 0: Light buffer (SSBO)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Binding 1: Tile light index buffer (SSBO)
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<u32>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_DescriptorLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create fragment descriptor set layout!");
    }

    // Create descriptor pool
    std::array<VkDescriptorPoolSize, 1> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[0].descriptorCount = 2;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create fragment descriptor pool!");
    }

    // Allocate descriptor set
    m_DescriptorSets.resize(1);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_DescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_DescriptorLayout;

    if (vkAllocateDescriptorSets(device, &allocInfo, m_DescriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate fragment descriptor sets!");
    }

    // Update descriptor sets
    std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

    VkDescriptorBufferInfo lightBufferInfo{};
    lightBufferInfo.buffer = m_LightBuffer;
    lightBufferInfo.offset = 0;
    lightBufferInfo.range = m_LightBufferSize;

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = m_DescriptorSets[0];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &lightBufferInfo;

    VkDescriptorBufferInfo tileBufferInfo{};
    tileBufferInfo.buffer = m_TileLightIndexBuffer;
    tileBufferInfo.offset = 0;
    tileBufferInfo.range = VK_WHOLE_SIZE;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = m_DescriptorSets[0];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &tileBufferInfo;

    vkUpdateDescriptorSets(device, static_cast<u32>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void VulkanLightCulling::DestroyBuffers() {
    if (!m_Context) return;

    VkDevice device = m_Context->GetDevice();

    if (m_DepthSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_DepthSampler, nullptr);
        m_DepthSampler = VK_NULL_HANDLE;
    }

    if (m_LightBuffer != VK_NULL_HANDLE) {
        vkUnmapMemory(device, m_LightBufferMemory);
        vkDestroyBuffer(device, m_LightBuffer, nullptr);
        vkFreeMemory(device, m_LightBufferMemory, nullptr);
        m_LightBuffer = VK_NULL_HANDLE;
        m_LightBufferMemory = VK_NULL_HANDLE;
        m_LightBufferMapped = nullptr;
    }

    if (m_TileLightIndexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_TileLightIndexBuffer, nullptr);
        vkFreeMemory(device, m_TileLightIndexMemory, nullptr);
        m_TileLightIndexBuffer = VK_NULL_HANDLE;
        m_TileLightIndexMemory = VK_NULL_HANDLE;
    }

    if (m_CullingParamsBuffer != VK_NULL_HANDLE) {
        vkUnmapMemory(device, m_CullingParamsMemory);
        vkDestroyBuffer(device, m_CullingParamsBuffer, nullptr);
        vkFreeMemory(device, m_CullingParamsMemory, nullptr);
        m_CullingParamsBuffer = VK_NULL_HANDLE;
        m_CullingParamsMemory = VK_NULL_HANDLE;
        m_CullingParamsMapped = nullptr;
    }
}

void VulkanLightCulling::DestroyComputePipeline() {
    if (!m_Context) return;

    VkDevice device = m_Context->GetDevice();

    if (m_ComputePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_ComputePipeline, nullptr);
        m_ComputePipeline = VK_NULL_HANDLE;
    }

    if (m_ComputePipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_ComputePipelineLayout, nullptr);
        m_ComputePipelineLayout = VK_NULL_HANDLE;
    }

    if (m_ComputeDescriptorLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_ComputeDescriptorLayout, nullptr);
        m_ComputeDescriptorLayout = VK_NULL_HANDLE;
    }

    if (m_ComputeDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_ComputeDescriptorPool, nullptr);
        m_ComputeDescriptorPool = VK_NULL_HANDLE;
    }
}

void VulkanLightCulling::DestroyDescriptorSets() {
    if (!m_Context) return;

    VkDevice device = m_Context->GetDevice();

    if (m_DescriptorLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_DescriptorLayout, nullptr);
        m_DescriptorLayout = VK_NULL_HANDLE;
    }

    if (m_DescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
        m_DescriptorPool = VK_NULL_HANDLE;
    }

    m_DescriptorSets.clear();
}

void VulkanLightCulling::UpdateLightBufferDescriptors() {
    VkDevice device = m_Context->GetDevice();

    VkDescriptorBufferInfo lightBufferInfo{};
    lightBufferInfo.buffer = m_LightBuffer;
    lightBufferInfo.offset = 0;
    lightBufferInfo.range = m_LightBufferSize;

    // Update compute descriptor sets for all frames
    for (u32 i = 0; i < m_FramesInFlight; ++i) {
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_ComputeDescriptorSets[i];
        write.dstBinding = 2;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &lightBufferInfo;

        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }

    // Update fragment descriptor set
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_DescriptorSets[0];
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &lightBufferInfo;

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

void VulkanLightCulling::CreateTimestampQueries() {
    VkDevice device = m_Context->GetDevice();

    // Get timestamp period from physical device properties
    VkPhysicalDeviceProperties deviceProps{};
    vkGetPhysicalDeviceProperties(m_Context->GetPhysicalDevice(), &deviceProps);
    m_TimestampPeriod = deviceProps.limits.timestampPeriod;

    // Check if timestamps are supported
    if (!deviceProps.limits.timestampComputeAndGraphics) {
        // Timestamps not supported, skip creation
        m_TimestampQueryPool = VK_NULL_HANDLE;
        return;
    }

    // Create query pool for timestamps (2 queries per frame: start and end)
    VkQueryPoolCreateInfo queryPoolInfo{};
    queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queryPoolInfo.queryCount = m_FramesInFlight * 2;

    if (vkCreateQueryPool(device, &queryPoolInfo, nullptr, &m_TimestampQueryPool) != VK_SUCCESS) {
        // Failed to create query pool, continue without timestamps
        m_TimestampQueryPool = VK_NULL_HANDLE;
    }
}

void VulkanLightCulling::DestroyTimestampQueries() {
    if (!m_Context || m_TimestampQueryPool == VK_NULL_HANDLE) return;

    VkDevice device = m_Context->GetDevice();
    vkDestroyQueryPool(device, m_TimestampQueryPool, nullptr);
    m_TimestampQueryPool = VK_NULL_HANDLE;
}

void VulkanLightCulling::UpdateTimestampResults() {
    if (m_TimestampQueryPool == VK_NULL_HANDLE) return;

    VkDevice device = m_Context->GetDevice();

    // Read timestamps from all frames (non-blocking, may fail if not ready)
    u64 timestamps[8] = {};  // Max 4 frames in flight * 2 timestamps
    VkResult result = vkGetQueryPoolResults(
        device,
        m_TimestampQueryPool,
        0,
        m_FramesInFlight * 2,
        sizeof(timestamps),
        timestamps,
        sizeof(u64),
        VK_QUERY_RESULT_64_BIT  // Don't wait, just get available results
    );

    // If we got valid results, compute the average time
    if (result == VK_SUCCESS) {
        f32 totalTime = 0.0f;
        u32 validFrames = 0;

        for (u32 i = 0; i < m_FramesInFlight; ++i) {
            u64 startTime = timestamps[i * 2];
            u64 endTime = timestamps[i * 2 + 1];

            // Only count if both timestamps are valid (non-zero)
            if (startTime > 0 && endTime > startTime) {
                f32 timeMs = static_cast<f32>(endTime - startTime) * m_TimestampPeriod / 1000000.0f;
                totalTime += timeMs;
                ++validFrames;
            }
        }

        // Update average time if we have valid data
        if (validFrames > 0) {
            m_LastCullingTimeMs = totalTime / static_cast<f32>(validFrames);
        }
    }
}
