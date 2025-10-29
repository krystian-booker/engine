#include "renderer/vulkan_descriptors.h"

#include "renderer/uniform_buffers.h"
#include "renderer/vulkan_context.h"
#include "renderer/vulkan_descriptor_pools.h"

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
    m_FramesInFlight = framesInFlight;

    // Initialize descriptor pools
    m_Context->GetDescriptorPools()->Init(m_Context, framesInFlight);

    CreateDescriptorSetLayouts();
    CreateUniformBuffers(framesInFlight);
    CreateDescriptorSets(framesInFlight);
}

void VulkanDescriptors::Shutdown() {
    if (!m_Context) {
        m_UniformBuffers.clear();
        m_TransientSets.clear();
        m_PersistentSet = VK_NULL_HANDLE;
        m_TransientLayout = VK_NULL_HANDLE;
        m_PersistentLayout = VK_NULL_HANDLE;
        return;
    }

    VkDevice device = m_Context->GetDevice();

    for (auto& buffer : m_UniformBuffers) {
        buffer.Destroy();
    }
    m_UniformBuffers.clear();

    // Descriptor sets are managed by VulkanDescriptorPools, so we just clear references
    m_TransientSets.clear();
    m_PersistentSet = VK_NULL_HANDLE;

    if (m_TransientLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_TransientLayout, nullptr);
        m_TransientLayout = VK_NULL_HANDLE;
    }

    if (m_PersistentLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_PersistentLayout, nullptr);
        m_PersistentLayout = VK_NULL_HANDLE;
    }

    m_Context = nullptr;
}

void VulkanDescriptors::CreateDescriptorSetLayouts() {
    VkDevice device = m_Context->GetDevice();

    // ===== Set 0: Transient (per-frame camera UBO) =====
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo transientLayoutInfo{};
    transientLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    transientLayoutInfo.bindingCount = 1;
    transientLayoutInfo.pBindings = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(device, &transientLayoutInfo, nullptr, &m_TransientLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create transient descriptor set layout");
    }

    // ===== Set 1: Persistent (material SSBO + bindless textures) =====

    // Binding 0: Material SSBO
    VkDescriptorSetLayoutBinding materialSSBOBinding{};
    materialSSBOBinding.binding = 0;
    materialSSBOBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    materialSSBOBinding.descriptorCount = 1;
    materialSSBOBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Binding 1: Bindless texture array
    VkDescriptorSetLayoutBinding bindlessTextureBinding{};
    bindlessTextureBinding.binding = 1;
    bindlessTextureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindlessTextureBinding.descriptorCount = MAX_BINDLESS_TEXTURES;
    bindlessTextureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindlessTextureBinding.pImmutableSamplers = nullptr;

    std::array<VkDescriptorSetLayoutBinding, 2> persistentBindings = {
        materialSSBOBinding,
        bindlessTextureBinding
    };

    // Binding flags for descriptor indexing
    std::array<VkDescriptorBindingFlags, 2> bindingFlags = {
        0,  // Binding 0 (Material SSBO) - no special flags
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT  // Binding 1 (bindless array)
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
    bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlagsInfo.bindingCount = static_cast<u32>(bindingFlags.size());
    bindingFlagsInfo.pBindingFlags = bindingFlags.data();

    VkDescriptorSetLayoutCreateInfo persistentLayoutInfo{};
    persistentLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    persistentLayoutInfo.bindingCount = static_cast<u32>(persistentBindings.size());
    persistentLayoutInfo.pBindings = persistentBindings.data();
    persistentLayoutInfo.pNext = &bindingFlagsInfo;
    persistentLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

    if (vkCreateDescriptorSetLayout(device, &persistentLayoutInfo, nullptr, &m_PersistentLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create persistent descriptor set layout");
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

void VulkanDescriptors::CreateDescriptorSets(u32 framesInFlight) {
    VulkanDescriptorPools* pools = m_Context->GetDescriptorPools();
    VkDevice device = m_Context->GetDevice();

    m_TransientSets.resize(framesInFlight);

    // Allocate transient descriptor sets (one per frame)
    for (u32 i = 0; i < framesInFlight; ++i) {
        m_TransientSets[i] = pools->AllocateDescriptorSet(
            m_TransientLayout,
            VulkanDescriptorPools::PoolType::Transient,
            i
        );

        if (m_TransientSets[i] == VK_NULL_HANDLE) {
            throw std::runtime_error("Failed to allocate transient descriptor set");
        }

        // Bind UBO to transient set
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_UniformBuffers[i].GetBuffer();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_TransientSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    }

    // Allocate persistent descriptor set (single, shared across all frames)
    m_PersistentSet = pools->AllocateDescriptorSet(
        m_PersistentLayout,
        VulkanDescriptorPools::PoolType::Persistent,
        0
    );

    if (m_PersistentSet == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to allocate persistent descriptor set");
    }

    // Material SSBO (binding 0) and bindless textures (binding 1) will be bound later
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

    // Update persistent descriptor set only (binding 1)
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = imageView;
    imageInfo.sampler = sampler;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = m_PersistentSet;  // Only update persistent set!
    descriptorWrite.dstBinding = 1;  // Bindless texture array is binding 1 in Set 1
    descriptorWrite.dstArrayElement = descriptorIndex;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_Context->GetDevice(), 1, &descriptorWrite, 0, nullptr);

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

    // Update persistent descriptor set only (binding 0)
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = buffer;
    bufferInfo.offset = offset;
    bufferInfo.range = range;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = m_PersistentSet;  // Only update persistent set!
    descriptorWrite.dstBinding = 0;  // Material SSBO is binding 0 in Set 1
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(m_Context->GetDevice(), 1, &descriptorWrite, 0, nullptr);
}
