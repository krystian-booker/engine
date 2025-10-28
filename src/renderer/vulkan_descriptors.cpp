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

    // Binding 1: Texture sampler (combined image sampler)
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    samplerLayoutBinding.pImmutableSamplers = nullptr;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<u32>(bindings.size());
    layoutInfo.pBindings = bindings.data();

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
    std::array<VkDescriptorPoolSize, 2> poolSizes{};

    // UBO pool
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = framesInFlight;

    // Texture sampler pool
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = framesInFlight;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
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

void VulkanDescriptors::BindTexture(u32 currentFrame, u32 binding, VkImageView imageView, VkSampler sampler) {
    if (currentFrame >= m_DescriptorSets.size()) {
        throw std::out_of_range("VulkanDescriptors::BindTexture frame index out of range");
    }

    if (!imageView || !sampler) {
        throw std::invalid_argument("VulkanDescriptors::BindTexture requires valid imageView and sampler");
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = imageView;
    imageInfo.sampler = sampler;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = m_DescriptorSets[currentFrame];
    descriptorWrite.dstBinding = binding;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_Context->GetDevice(), 1, &descriptorWrite, 0, nullptr);
}

void VulkanDescriptors::BindTextureArray(u32 currentFrame, u32 binding, VkImageView imageView, VkSampler sampler) {
    // Array textures use the same binding method as regular textures
    // The difference is in the image view type (VK_IMAGE_VIEW_TYPE_2D_ARRAY)
    // which is set when the VulkanTexture is created
    BindTexture(currentFrame, binding, imageView, sampler);
}

