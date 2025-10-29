#include "renderer/vulkan_descriptors.h"

#include "renderer/uniform_buffers.h"
#include "renderer/vulkan_context.h"

#include <array>
#include <stdexcept>

VulkanDescriptors::~VulkanDescriptors() {
    Shutdown();
}

void VulkanDescriptors::Init(VulkanContext* context, u32 framesInFlight) {
    if (!context) {
        throw std::invalid_argument("VulkanDescriptors::Init requires a valid context");
    }

    Shutdown();

    m_Context = context;

    CreateDescriptorSetLayout();
    CreateUniformBuffers(framesInFlight);
    CreateDescriptorPool(framesInFlight);
    CreateDescriptorSets(framesInFlight);
}

void VulkanDescriptors::Shutdown() {
    if (!m_Context) {
        m_UniformBuffers.clear();
        m_DescriptorSets.clear();
        m_DescriptorPool = VK_NULL_HANDLE;
        m_DescriptorSetLayout = VK_NULL_HANDLE;
        return;
    }

    VkDevice device = m_Context->GetDevice();

    for (auto& buffer : m_UniformBuffers) {
        buffer.Destroy();
    }
    m_UniformBuffers.clear();

    if (m_DescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
        m_DescriptorPool = VK_NULL_HANDLE;
    }

    if (m_DescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_DescriptorSetLayout, nullptr);
        m_DescriptorSetLayout = VK_NULL_HANDLE;
    }

    m_DescriptorSets.clear();
    m_Context = nullptr;
}

void VulkanDescriptors::CreateDescriptorSetLayout() {
    // Binding 0: UBO (MVP matrices)
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    // Binding 1: Material SSBO
    VkDescriptorSetLayoutBinding materialSSBOBinding{};
    materialSSBOBinding.binding = 1;
    materialSSBOBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    materialSSBOBinding.descriptorCount = 1;
    materialSSBOBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Binding 2: Bindless texture array (large descriptor count)
    VkDescriptorSetLayoutBinding bindlessTextureBinding{};
    bindlessTextureBinding.binding = 2;
    bindlessTextureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindlessTextureBinding.descriptorCount = MAX_BINDLESS_TEXTURES;
    bindlessTextureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindlessTextureBinding.pImmutableSamplers = nullptr;

    std::array<VkDescriptorSetLayoutBinding, 3> bindings = {
        uboLayoutBinding,
        materialSSBOBinding,
        bindlessTextureBinding
    };

    // Binding flags for descriptor indexing features
    std::array<VkDescriptorBindingFlags, 3> bindingFlags = {
        0,  // Binding 0 (UBO) - no special flags
        0,  // Binding 1 (Material SSBO) - no special flags
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT  // Binding 2 (bindless array)
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
    bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlagsInfo.bindingCount = static_cast<u32>(bindingFlags.size());
    bindingFlagsInfo.pBindingFlags = bindingFlags.data();

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<u32>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    layoutInfo.pNext = &bindingFlagsInfo;

    if (vkCreateDescriptorSetLayout(m_Context->GetDevice(), &layoutInfo, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout");
    }
}

void VulkanDescriptors::CreateUniformBuffers(u32 framesInFlight) {
    if (framesInFlight == 0) {
        throw std::invalid_argument("VulkanDescriptors::CreateUniformBuffers requires at least one frame");
    }

    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    m_UniformBuffers.resize(framesInFlight);

    for (u32 i = 0; i < framesInFlight; ++i) {
        m_UniformBuffers[i].Create(
            m_Context,
            bufferSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }
}

void VulkanDescriptors::UpdateUniformBuffer(u32 currentFrame, const void* data, size_t size) {
    if (currentFrame >= m_UniformBuffers.size()) {
        throw std::out_of_range("VulkanDescriptors::UpdateUniformBuffer frame index out of range");
    }

    if (size > m_UniformBuffers[currentFrame].GetSize()) {
        throw std::runtime_error("VulkanDescriptors::UpdateUniformBuffer size exceeds buffer capacity");
    }

    m_UniformBuffers[currentFrame].CopyFrom(data, size);
}

void VulkanDescriptors::CreateDescriptorPool(u32 framesInFlight) {
    std::array<VkDescriptorPoolSize, 3> poolSizes{};

    // UBO pool
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = framesInFlight;

    // Material SSBO pool
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = framesInFlight;

    // Bindless texture array pool (large)
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[2].descriptorCount = MAX_BINDLESS_TEXTURES * framesInFlight;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = framesInFlight;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;  // Required for bindless

    if (vkCreateDescriptorPool(m_Context->GetDevice(), &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool");
    }
}

void VulkanDescriptors::CreateDescriptorSets(u32 framesInFlight) {
    m_DescriptorSets.resize(framesInFlight);

    std::vector<VkDescriptorSetLayout> layouts(framesInFlight, m_DescriptorSetLayout);

    // Variable descriptor count for bindless array (binding 2)
    std::vector<u32> variableDescriptorCounts(framesInFlight, MAX_BINDLESS_TEXTURES);

    VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountInfo{};
    variableDescriptorCountInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    variableDescriptorCountInfo.descriptorSetCount = framesInFlight;
    variableDescriptorCountInfo.pDescriptorCounts = variableDescriptorCounts.data();

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_DescriptorPool;
    allocInfo.descriptorSetCount = framesInFlight;
    allocInfo.pSetLayouts = layouts.data();
    allocInfo.pNext = &variableDescriptorCountInfo;

    if (vkAllocateDescriptorSets(m_Context->GetDevice(), &allocInfo, m_DescriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets");
    }

    // Write UBO descriptor (binding 0) for each frame
    for (u32 i = 0; i < framesInFlight; ++i) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_UniformBuffers[i].GetBuffer();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_DescriptorSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(m_Context->GetDevice(), 1, &descriptorWrite, 0, nullptr);
    }

    // Material SSBO (binding 1) will be bound later via BindMaterialBuffer()
    // Bindless textures (binding 2) will be registered via RegisterTexture()
}

u32 VulkanDescriptors::RegisterTexture(VkImageView imageView, VkSampler sampler) {
    if (!imageView || !sampler) {
        throw std::invalid_argument("VulkanDescriptors::RegisterTexture requires valid imageView and sampler");
    }

    // Allocate descriptor index
    u32 descriptorIndex;
    if (!m_FreeTextureIndices.empty()) {
        descriptorIndex = m_FreeTextureIndices.front();
        m_FreeTextureIndices.pop();
    } else {
        descriptorIndex = m_NextTextureIndex++;
        if (descriptorIndex >= MAX_BINDLESS_TEXTURES) {
            throw std::runtime_error("VulkanDescriptors::RegisterTexture exceeded MAX_BINDLESS_TEXTURES");
        }
    }

    // Update all descriptor sets (all frames in flight)
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = imageView;
    imageInfo.sampler = sampler;

    for (VkDescriptorSet descriptorSet : m_DescriptorSets) {
        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSet;
        descriptorWrite.dstBinding = 2;  // Bindless texture array is binding 2
        descriptorWrite.dstArrayElement = descriptorIndex;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(m_Context->GetDevice(), 1, &descriptorWrite, 0, nullptr);
    }

    return descriptorIndex;
}

void VulkanDescriptors::UnregisterTexture(u32 descriptorIndex) {
    if (descriptorIndex >= MAX_BINDLESS_TEXTURES) {
        throw std::out_of_range("VulkanDescriptors::UnregisterTexture descriptor index out of range");
    }

    // Return index to free list for reuse
    m_FreeTextureIndices.push(descriptorIndex);
}

void VulkanDescriptors::BindMaterialBuffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range) {
    if (!buffer) {
        throw std::invalid_argument("VulkanDescriptors::BindMaterialBuffer requires valid buffer");
    }

    // Update all descriptor sets (all frames in flight)
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = buffer;
    bufferInfo.offset = offset;
    bufferInfo.range = range;

    for (VkDescriptorSet descriptorSet : m_DescriptorSets) {
        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSet;
        descriptorWrite.dstBinding = 1;  // Material SSBO is binding 1
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(m_Context->GetDevice(), 1, &descriptorWrite, 0, nullptr);
    }
}

