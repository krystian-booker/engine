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

    for (auto& buffer : m_LightingBuffers) {
        buffer.Destroy();
    }
    m_LightingBuffers.clear();

    for (auto& buffer : m_ShadowBuffers) {
        buffer.Destroy();
    }
    m_ShadowBuffers.clear();

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

    // ===== Set 0: Transient (per-frame UBOs + shadow/IBL textures) =====
    // Note: Must match shader bindings in cube.frag

    // Binding 0: Camera view/projection UBO
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    // Binding 1: Lighting UBO
    VkDescriptorSetLayoutBinding lightingLayoutBinding{};
    lightingLayoutBinding.binding = 1;
    lightingLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    lightingLayoutBinding.descriptorCount = 1;
    lightingLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Binding 2: Shadow UBO
    VkDescriptorSetLayoutBinding shadowUBOBinding{};
    shadowUBOBinding.binding = 2;
    shadowUBOBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    shadowUBOBinding.descriptorCount = 1;
    shadowUBOBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Binding 3: Shadow map array (directional/CSM)
    VkDescriptorSetLayoutBinding shadowMapBinding{};
    shadowMapBinding.binding = 3;
    shadowMapBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    shadowMapBinding.descriptorCount = 1;
    shadowMapBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Binding 4: IBL Irradiance cubemap
    VkDescriptorSetLayoutBinding irradianceBinding{};
    irradianceBinding.binding = 4;
    irradianceBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    irradianceBinding.descriptorCount = 1;
    irradianceBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Binding 5: IBL Prefiltered environment map
    VkDescriptorSetLayoutBinding prefilteredBinding{};
    prefilteredBinding.binding = 5;
    prefilteredBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    prefilteredBinding.descriptorCount = 1;
    prefilteredBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Binding 6: IBL BRDF LUT
    VkDescriptorSetLayoutBinding brdfBinding{};
    brdfBinding.binding = 6;
    brdfBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    brdfBinding.descriptorCount = 1;
    brdfBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Binding 7: Point light shadow cubemap array
    VkDescriptorSetLayoutBinding pointShadowBinding{};
    pointShadowBinding.binding = 7;
    pointShadowBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pointShadowBinding.descriptorCount = 1;
    pointShadowBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Binding 8: Spot light shadow map array
    VkDescriptorSetLayoutBinding spotShadowBinding{};
    spotShadowBinding.binding = 8;
    spotShadowBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    spotShadowBinding.descriptorCount = 1;
    spotShadowBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Binding 9: EVSM moment texture array
    VkDescriptorSetLayoutBinding evsmBinding{};
    evsmBinding.binding = 9;
    evsmBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    evsmBinding.descriptorCount = 1;
    evsmBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Binding 10: Raw depth shadow map (non-comparison sampler for PCSS blocker search)
    VkDescriptorSetLayoutBinding rawDepthBinding{};
    rawDepthBinding.binding = 10;
    rawDepthBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    rawDepthBinding.descriptorCount = 1;
    rawDepthBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 11> transientBindings = {
        uboLayoutBinding,
        lightingLayoutBinding,
        shadowUBOBinding,
        shadowMapBinding,
        irradianceBinding,
        prefilteredBinding,
        brdfBinding,
        pointShadowBinding,
        spotShadowBinding,
        evsmBinding,
        rawDepthBinding
    };

    VkDescriptorSetLayoutCreateInfo transientLayoutInfo{};
    transientLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    transientLayoutInfo.bindingCount = static_cast<u32>(transientBindings.size());
    transientLayoutInfo.pBindings = transientBindings.data();

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

    // Create camera uniform buffers
    VkDeviceSize cameraBufferSize = sizeof(UniformBufferObject);
    m_UniformBuffers.resize(framesInFlight);

    for (u32 i = 0; i < framesInFlight; ++i) {
        m_UniformBuffers[i].Create(
            m_Context,
            cameraBufferSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }

    // Create lighting uniform buffers
    VkDeviceSize lightingBufferSize = sizeof(LightingUniformBuffer);
    m_LightingBuffers.resize(framesInFlight);

    for (u32 i = 0; i < framesInFlight; ++i) {
        m_LightingBuffers[i].Create(
            m_Context,
            lightingBufferSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }

    // Create shadow uniform buffers
    VkDeviceSize shadowBufferSize = sizeof(ShadowUniforms);
    m_ShadowBuffers.resize(framesInFlight);

    for (u32 i = 0; i < framesInFlight; ++i) {
        m_ShadowBuffers[i].Create(
            m_Context,
            shadowBufferSize,
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

void VulkanDescriptors::UpdateLightingBuffer(u32 currentFrame, const void* data, size_t size) {
    if (currentFrame >= m_LightingBuffers.size()) {
        throw std::out_of_range("VulkanDescriptors::UpdateLightingBuffer frame index out of range");
    }

    if (size > m_LightingBuffers[currentFrame].GetSize()) {
        throw std::runtime_error("VulkanDescriptors::UpdateLightingBuffer size exceeds buffer capacity");
    }

    m_LightingBuffers[currentFrame].CopyFrom(data, size);
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

        // Bind UBO to transient set (binding 0 - camera)
        VkDescriptorBufferInfo cameraBufferInfo{};
        cameraBufferInfo.buffer = m_UniformBuffers[i].GetBuffer();
        cameraBufferInfo.offset = 0;
        cameraBufferInfo.range = sizeof(UniformBufferObject);

        // Bind lighting UBO to transient set (binding 1 - lighting)
        VkDescriptorBufferInfo lightingBufferInfo{};
        lightingBufferInfo.buffer = m_LightingBuffers[i].GetBuffer();
        lightingBufferInfo.offset = 0;
        lightingBufferInfo.range = sizeof(LightingUniformBuffer);

        // Bind shadow UBO to transient set (binding 2 - shadow)
        VkDescriptorBufferInfo shadowBufferInfo{};
        shadowBufferInfo.buffer = m_ShadowBuffers[i].GetBuffer();
        shadowBufferInfo.offset = 0;
        shadowBufferInfo.range = sizeof(ShadowUniforms);

        std::array<VkWriteDescriptorSet, 3> descriptorWrites{};

        // Camera UBO (binding 0)
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = m_TransientSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &cameraBufferInfo;

        // Lighting UBO (binding 1)
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = m_TransientSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &lightingBufferInfo;

        // Shadow UBO (binding 2)
        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = m_TransientSets[i];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &shadowBufferInfo;

        vkUpdateDescriptorSets(device, static_cast<u32>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }

    // Allocate persistent descriptor set (single, shared across all frames)
    // Pass MAX_BINDLESS_TEXTURES as the variable descriptor count for binding 1
    m_PersistentSet = pools->AllocateDescriptorSet(
        m_PersistentLayout,
        VulkanDescriptorPools::PoolType::Persistent,
        0,  // frameIndex (not used for persistent)
        MAX_BINDLESS_TEXTURES  // Variable descriptor count for bindless texture array
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

// Helper function to bind image sampler to all transient sets
static void BindImageToAllFrames(VkDevice device, const std::vector<VkDescriptorSet>& sets,
                                  u32 binding, VkImageView imageView, VkSampler sampler) {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = imageView;
    imageInfo.sampler = sampler;

    for (size_t i = 0; i < sets.size(); ++i) {
        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = sets[i];
        descriptorWrite.dstBinding = binding;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    }
}

void VulkanDescriptors::BindShadowUBO(u32 currentFrame, const void* data, size_t size) {
    if (currentFrame >= m_ShadowBuffers.size()) {
        throw std::out_of_range("VulkanDescriptors::BindShadowUBO frame index out of range");
    }

    if (size > m_ShadowBuffers[currentFrame].GetSize()) {
        throw std::runtime_error("VulkanDescriptors::BindShadowUBO size exceeds buffer capacity");
    }

    m_ShadowBuffers[currentFrame].CopyFrom(data, size);
}

void VulkanDescriptors::BindShadowMap(VkImageView imageView, VkSampler sampler) {
    if (!imageView || !sampler) {
        throw std::invalid_argument("VulkanDescriptors::BindShadowMap requires valid imageView and sampler");
    }
    BindImageToAllFrames(m_Context->GetDevice(), m_TransientSets, 3, imageView, sampler);
}

void VulkanDescriptors::BindIBLIrradiance(VkImageView imageView, VkSampler sampler) {
    if (!imageView || !sampler) {
        throw std::invalid_argument("VulkanDescriptors::BindIBLIrradiance requires valid imageView and sampler");
    }
    BindImageToAllFrames(m_Context->GetDevice(), m_TransientSets, 4, imageView, sampler);
}

void VulkanDescriptors::BindIBLPrefiltered(VkImageView imageView, VkSampler sampler) {
    if (!imageView || !sampler) {
        throw std::invalid_argument("VulkanDescriptors::BindIBLPrefiltered requires valid imageView and sampler");
    }
    BindImageToAllFrames(m_Context->GetDevice(), m_TransientSets, 5, imageView, sampler);
}

void VulkanDescriptors::BindIBLBRDF(VkImageView imageView, VkSampler sampler) {
    if (!imageView || !sampler) {
        throw std::invalid_argument("VulkanDescriptors::BindIBLBRDF requires valid imageView and sampler");
    }
    BindImageToAllFrames(m_Context->GetDevice(), m_TransientSets, 6, imageView, sampler);
}

void VulkanDescriptors::BindPointShadowMaps(VkImageView imageView, VkSampler sampler) {
    if (!imageView || !sampler) {
        throw std::invalid_argument("VulkanDescriptors::BindPointShadowMaps requires valid imageView and sampler");
    }
    BindImageToAllFrames(m_Context->GetDevice(), m_TransientSets, 7, imageView, sampler);
}

void VulkanDescriptors::BindSpotShadowMaps(VkImageView imageView, VkSampler sampler) {
    if (!imageView || !sampler) {
        throw std::invalid_argument("VulkanDescriptors::BindSpotShadowMaps requires valid imageView and sampler");
    }
    BindImageToAllFrames(m_Context->GetDevice(), m_TransientSets, 8, imageView, sampler);
}

void VulkanDescriptors::BindEVSMShadows(VkImageView imageView, VkSampler sampler) {
    if (!imageView || !sampler) {
        throw std::invalid_argument("VulkanDescriptors::BindEVSMShadows requires valid imageView and sampler");
    }
    BindImageToAllFrames(m_Context->GetDevice(), m_TransientSets, 9, imageView, sampler);
}

void VulkanDescriptors::BindRawDepthShadowMap(VkImageView imageView, VkSampler sampler) {
    if (!imageView || !sampler) {
        throw std::invalid_argument("VulkanDescriptors::BindRawDepthShadowMap requires valid imageView and sampler");
    }
    BindImageToAllFrames(m_Context->GetDevice(), m_TransientSets, 10, imageView, sampler);
}
