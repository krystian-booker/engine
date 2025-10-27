#include "renderer/vulkan_texture.h"
#include "renderer/vulkan_context.h"
#include "renderer/vulkan_buffer.h"
#include "core/texture_data.h"
#include <stdexcept>
#include <algorithm>
#include <cmath>

// Removed unused helper function IsFormatSRGB (may be used in future for validation)

VulkanTexture::~VulkanTexture() {
    Destroy();
}

void VulkanTexture::Create(VulkanContext* context, const TextureData* textureData) {
    if (!context || !textureData) {
        throw std::invalid_argument("VulkanTexture::Create requires valid context and texture data");
    }

    if (!textureData->pixels || textureData->width == 0 || textureData->height == 0) {
        throw std::invalid_argument("VulkanTexture::Create requires valid pixel data");
    }

    // Clean up existing resources
    Destroy();

    m_Context = context;
    m_Format = DetermineVulkanFormat(textureData);

    // Calculate mip levels if requested
    if (HasFlag(textureData->flags, TextureFlags::GenerateMipmaps)) {
        m_MipLevels = static_cast<u32>(std::floor(std::log2(std::max(textureData->width, textureData->height)))) + 1;
    } else {
        m_MipLevels = 1;
    }

    // Create image, upload data, generate mipmaps
    CreateImage(textureData);
    CreateImageView();
    CreateSampler(textureData);
}

void VulkanTexture::Destroy() {
    if (!m_Context) {
        m_Image = VK_NULL_HANDLE;
        m_ImageMemory = VK_NULL_HANDLE;
        m_ImageView = VK_NULL_HANDLE;
        m_Sampler = VK_NULL_HANDLE;
        return;
    }

    const VkDevice device = m_Context->GetDevice();

    if (m_Sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_Sampler, nullptr);
        m_Sampler = VK_NULL_HANDLE;
    }

    if (m_ImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_ImageView, nullptr);
        m_ImageView = VK_NULL_HANDLE;
    }

    if (m_Image != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_Image, nullptr);
        m_Image = VK_NULL_HANDLE;
    }

    if (m_ImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_ImageMemory, nullptr);
        m_ImageMemory = VK_NULL_HANDLE;
    }

    m_Context = nullptr;
    m_Format = VK_FORMAT_UNDEFINED;
    m_MipLevels = 1;
}

void VulkanTexture::CreateImage(const TextureData* data) {
    const VkDevice device = m_Context->GetDevice();
    const VkDeviceSize imageSize = data->width * data->height * data->channels;

    // Create staging buffer
    VulkanBuffer stagingBuffer;
    stagingBuffer.Create(m_Context, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // Copy pixel data to staging buffer
    void* mappedData = stagingBuffer.Map();
    memcpy(mappedData, data->pixels, static_cast<size_t>(imageSize));
    stagingBuffer.Unmap();

    // Create Vulkan image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = data->width;
    imageInfo.extent.height = data->height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = m_MipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = m_Format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    // Add mipmap generation usage flags if needed
    if (m_MipLevels > 1) {
        imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &m_Image) != VK_SUCCESS) {
        stagingBuffer.Destroy();
        throw std::runtime_error("VulkanTexture::CreateImage failed to create VkImage");
    }

    // Allocate image memory
    VkMemoryRequirements memRequirements{};
    vkGetImageMemoryRequirements(device, m_Image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits,
                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_ImageMemory) != VK_SUCCESS) {
        stagingBuffer.Destroy();
        throw std::runtime_error("VulkanTexture::CreateImage failed to allocate image memory");
    }

    if (vkBindImageMemory(device, m_Image, m_ImageMemory, 0) != VK_SUCCESS) {
        stagingBuffer.Destroy();
        throw std::runtime_error("VulkanTexture::CreateImage failed to bind image memory");
    }

    // Transition image layout and copy staging buffer
    TransitionImageLayout(m_Image, m_Format, VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);
    CopyBufferToImage(stagingBuffer.GetBuffer(), m_Image, data->width, data->height);

    // Generate mipmaps or transition to shader read-only layout
    if (m_MipLevels > 1) {
        GenerateMipmaps(m_Image, m_Format, data->width, data->height, m_MipLevels);
    } else {
        TransitionImageLayout(m_Image, m_Format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
    }

    stagingBuffer.Destroy();
}

void VulkanTexture::CreateImageView() {
    const VkDevice device = m_Context->GetDevice();

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_Image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_Format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = m_MipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &m_ImageView) != VK_SUCCESS) {
        throw std::runtime_error("VulkanTexture::CreateImageView failed to create image view");
    }
}

void VulkanTexture::CreateSampler(const TextureData* data) {
    const VkDevice device = m_Context->GetDevice();
    const VkPhysicalDevice physicalDevice = m_Context->GetPhysicalDevice();

    // Query device properties for anisotropy support
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);

    // Determine anisotropy level
    f32 maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    f32 anisotropy = maxAnisotropy;  // Default to max

    // Check for per-texture override
    if (HasFlag(data->flags, TextureFlags::AnisotropyOverride) && data->anisotropyLevel > 0) {
        anisotropy = std::min(static_cast<f32>(data->anisotropyLevel), maxAnisotropy);
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = anisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<f32>(m_MipLevels);

    if (vkCreateSampler(device, &samplerInfo, nullptr, &m_Sampler) != VK_SUCCESS) {
        throw std::runtime_error("VulkanTexture::CreateSampler failed to create sampler");
    }
}

void VulkanTexture::GenerateMipmaps(VkImage image, VkFormat format, u32 width, u32 height, u32 mipLevels) {
    // Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(m_Context->GetPhysicalDevice(), format, &formatProperties);

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error("VulkanTexture::GenerateMipmaps texture format does not support linear blitting");
    }

    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    i32 mipWidth = static_cast<i32>(width);
    i32 mipHeight = static_cast<i32>(height);

    for (u32 i = 1; i < mipLevels; ++i) {
        // Transition previous mip level to transfer source
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
                            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                            0, nullptr,
                            0, nullptr,
                            1, &barrier);

        // Blit from previous mip level to current
        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1,
                              mipHeight > 1 ? mipHeight / 2 : 1,
                              1};
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(commandBuffer,
                      image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                      image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                      1, &blit,
                      VK_FILTER_LINEAR);

        // Transition previous mip level to shader read-only
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
                            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                            0, nullptr,
                            0, nullptr,
                            1, &barrier);

        // Halve dimensions for next iteration
        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    // Transition last mip level to shader read-only
    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
                        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                        0, nullptr,
                        0, nullptr,
                        1, &barrier);

    EndSingleTimeCommands(commandBuffer);
}

void VulkanTexture::TransitionImageLayout(VkImage image, VkFormat /*format*/, VkImageLayout oldLayout,
                                         VkImageLayout newLayout, u32 mipLevels) {
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::invalid_argument("VulkanTexture::TransitionImageLayout unsupported layout transition");
    }

    vkCmdPipelineBarrier(commandBuffer,
                        sourceStage, destinationStage,
                        0,
                        0, nullptr,
                        0, nullptr,
                        1, &barrier);

    EndSingleTimeCommands(commandBuffer);
}

void VulkanTexture::CopyBufferToImage(VkBuffer buffer, VkImage image, u32 width, u32 height) {
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    EndSingleTimeCommands(commandBuffer);
}

VkFormat VulkanTexture::DetermineVulkanFormat(const TextureData* data) const {
    // Check for explicit override
    if (data->formatOverride != VK_FORMAT_UNDEFINED) {
        return data->formatOverride;
    }

    // Auto-detect based on channels and sRGB requirement
    bool useSRGB = ShouldUseSRGB(data);

    switch (data->channels) {
        case 1:
            return useSRGB ? VK_FORMAT_R8_SRGB : VK_FORMAT_R8_UNORM;
        case 2:
            return useSRGB ? VK_FORMAT_R8G8_SRGB : VK_FORMAT_R8G8_UNORM;
        case 3:
            return useSRGB ? VK_FORMAT_R8G8B8_SRGB : VK_FORMAT_R8G8B8_UNORM;
        case 4:
            return useSRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
        default:
            throw std::runtime_error("VulkanTexture::DetermineVulkanFormat unsupported channel count");
    }
}

bool VulkanTexture::ShouldUseSRGB(const TextureData* data) const {
    // Check for explicit flag override
    if (HasFlag(data->flags, TextureFlags::SRGB)) {
        return true;
    }

    // Auto-detect based on usage
    switch (data->usage) {
        case TextureUsage::Albedo:
        case TextureUsage::AO:
            return true;  // These are typically sRGB
        case TextureUsage::Normal:
        case TextureUsage::Roughness:
        case TextureUsage::Metalness:
        case TextureUsage::Height:
            return false;  // These are always linear
        case TextureUsage::Generic:
        default:
            return false;  // Default to linear for unknown usage
    }
}

u32 VulkanTexture::FindMemoryType(u32 typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProperties{};
    vkGetPhysicalDeviceMemoryProperties(m_Context->GetPhysicalDevice(), &memProperties);

    for (u32 i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("VulkanTexture::FindMemoryType failed to find suitable memory type");
}

VkCommandBuffer VulkanTexture::BeginSingleTimeCommands() const {
    // Allocate temporary command buffer from graphics queue
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_Context->GetCommandPool();
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_Context->GetDevice(), &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void VulkanTexture::EndSingleTimeCommands(VkCommandBuffer commandBuffer) const {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(m_Context->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_Context->GetGraphicsQueue());

    vkFreeCommandBuffers(m_Context->GetDevice(), m_Context->GetCommandPool(), 1, &commandBuffer);
}
