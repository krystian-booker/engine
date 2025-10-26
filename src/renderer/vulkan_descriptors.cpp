#include "renderer/vulkan_descriptors.h"

#include "renderer/uniform_buffers.h"
#include "renderer/vulkan_context.h"

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
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

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
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = framesInFlight;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = framesInFlight;

    if (vkCreateDescriptorPool(m_Context->GetDevice(), &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool");
    }
}

void VulkanDescriptors::CreateDescriptorSets(u32 framesInFlight) {
    m_DescriptorSets.resize(framesInFlight);

    std::vector<VkDescriptorSetLayout> layouts(framesInFlight, m_DescriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_DescriptorPool;
    allocInfo.descriptorSetCount = framesInFlight;
    allocInfo.pSetLayouts = layouts.data();

    if (vkAllocateDescriptorSets(m_Context->GetDevice(), &allocInfo, m_DescriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets");
    }

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
}

