#include "renderer/vulkan_texture.h"
#include "renderer/vulkan_context.h"
#include "renderer/vulkan_mipmap_compute.h"
#include "renderer/vulkan_buffer.h"
#include "core/texture_data.h"
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace {
bool IsFormatSRGB(VkFormat format) {
    switch (format) {
        case VK_FORMAT_R8_SRGB:
        case VK_FORMAT_R8G8_SRGB:
        case VK_FORMAT_R8G8B8_SRGB:
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_B8G8R8A8_SRGB:
            return true;
        default:
            return false;
    }
}

VkFormat GetLinearFormatFor(VkFormat format) {
    switch (format) {
        case VK_FORMAT_R8_SRGB: return VK_FORMAT_R8_UNORM;
        case VK_FORMAT_R8G8_SRGB: return VK_FORMAT_R8G8_UNORM;
        case VK_FORMAT_R8G8B8_SRGB: return VK_FORMAT_R8G8B8_UNORM;
        case VK_FORMAT_R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_UNORM;
        case VK_FORMAT_B8G8R8A8_SRGB: return VK_FORMAT_B8G8R8A8_UNORM;
        default:
            return format;
    }
}
} // namespace

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
    m_Usage = textureData->usage;

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
    m_Usage = TextureUsage::Generic;
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
    imageInfo.flags = 0;

    const bool wantsMipmaps = m_MipLevels > 1;
    if (wantsMipmaps) {
        imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    if (wantsMipmaps) {
        const VkFormat storageCandidate = GetLinearFormatFor(m_Format);
        bool supportsStorage = m_Context->SupportsStorageImage(m_Format);
        if (!supportsStorage && storageCandidate != m_Format) {
            supportsStorage = m_Context->SupportsStorageImage(storageCandidate);
        }

        if (supportsStorage) {
            imageInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
            if (IsFormatSRGB(m_Format) && storageCandidate != m_Format) {
                imageInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
            }
        }
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
    // Multi-tier fallback system for mipmap generation
    // Tier 1: GPU blit-based (fastest, requires linear blit support)
    if (m_Context->SupportsLinearBlit(format)) {
        std::cout << "Using GPU blit for mipmap generation" << std::endl;
        GenerateMipmapsBlit(image, format, width, height, mipLevels);
        return;
    }

    // Tier 2: Compute shader-based (requires storage image support)
    const VkFormat storageCandidate = GetLinearFormatFor(format);
    bool supportsCompute = m_Context->SupportsStorageImage(format);
    if (!supportsCompute && storageCandidate != format) {
        supportsCompute = m_Context->SupportsStorageImage(storageCandidate);
    }

    if (supportsCompute) {
        std::cout << "Linear blit unsupported, using compute shader for mipmap generation" << std::endl;
        GenerateMipmapsCompute(image, format, width, height, mipLevels);
        return;
    }

    // Tier 3: CPU-based (slow but always works)
    std::cout << "WARNING: Compute shader unsupported, using CPU-based mipmap generation (slow)" << std::endl;
    GenerateMipmapsCPU(image, format, width, height, mipLevels);
}

void VulkanTexture::GenerateMipmapsBlit(VkImage image, VkFormat /*format*/, u32 width, u32 height, u32 mipLevels) {
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

void VulkanTexture::GenerateMipmapsCompute(VkImage image, VkFormat format, u32 width, u32 height, u32 mipLevels) {
    VulkanMipmapCompute* mipmapCompute = m_Context->GetMipmapCompute();
    if (!mipmapCompute) {
        throw std::runtime_error("VulkanTexture::GenerateMipmapsCompute requires initialized compute subsystem");
    }

    VulkanMipmapCompute::Variant variant = VulkanMipmapCompute::Variant::Color;
    switch (m_Usage) {
        case TextureUsage::Normal:
            variant = VulkanMipmapCompute::Variant::Normal;
            break;
        case TextureUsage::Roughness:
            variant = VulkanMipmapCompute::Variant::Roughness;
            break;
        default:
            variant = IsFormatSRGB(format) ? VulkanMipmapCompute::Variant::Srgb
                                           : VulkanMipmapCompute::Variant::Color;
            break;
    }

    VulkanMipmapCompute::Params params{};
    params.image = image;
    params.format = format;
    params.width = width;
    params.height = height;
    params.mipLevels = mipLevels;
    params.baseArrayLayer = 0;
    params.layerCount = 1;
    params.variant = variant;
    params.hasNormalMap = false;
    params.normalImage = VK_NULL_HANDLE;
    params.normalFormat = VK_FORMAT_UNDEFINED;

    mipmapCompute->Generate(params);
}

void VulkanTexture::GenerateMipmapsCPU(VkImage image, VkFormat format, u32 width, u32 height, u32 mipLevels) {
    // Determine bytes per pixel based on format
    u32 bytesPerPixel = 0;
    bool isSRGB = false;

    switch (format) {
        case VK_FORMAT_R8_UNORM:
        case VK_FORMAT_R8_SRGB:
            bytesPerPixel = 1;
            isSRGB = (format == VK_FORMAT_R8_SRGB);
            break;
        case VK_FORMAT_R8G8_UNORM:
        case VK_FORMAT_R8G8_SRGB:
            bytesPerPixel = 2;
            isSRGB = (format == VK_FORMAT_R8G8_SRGB);
            break;
        case VK_FORMAT_R8G8B8_UNORM:
        case VK_FORMAT_R8G8B8_SRGB:
            bytesPerPixel = 3;
            isSRGB = (format == VK_FORMAT_R8G8B8_SRGB);
            break;
        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_B8G8R8A8_SRGB:
            bytesPerPixel = 4;
            isSRGB = (format == VK_FORMAT_R8G8B8A8_SRGB || format == VK_FORMAT_B8G8R8A8_SRGB);
            break;
        default:
            throw std::runtime_error("VulkanTexture::GenerateMipmapsCPU unsupported format for CPU mipmap generation");
    }

    // Allocate CPU buffers for mip levels
    std::vector<std::vector<u8>> mipData(mipLevels);

    u32 mipWidth = width;
    u32 mipHeight = height;

    for (u32 level = 0; level < mipLevels; ++level) {
        const VkDeviceSize mipSize = mipWidth * mipHeight * bytesPerPixel;
        mipData[level].resize(static_cast<size_t>(mipSize));

        if (level == 0) {
            // Download mip level 0 from GPU
            VulkanBuffer stagingBuffer;
            stagingBuffer.Create(m_Context, mipSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            // Transition image to transfer source
            TransitionImageLayout(image, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 1);

            // Copy image to staging buffer
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
            region.imageExtent = {mipWidth, mipHeight, 1};

            vkCmdCopyImageToBuffer(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                 stagingBuffer.GetBuffer(), 1, &region);

            EndSingleTimeCommands(commandBuffer);

            // Copy to CPU buffer
            void* data = stagingBuffer.Map();
            memcpy(mipData[0].data(), data, static_cast<size_t>(mipSize));
            stagingBuffer.Unmap();
            stagingBuffer.Destroy();
        } else {
            // Generate from previous mip level using box filter
            const u32 prevWidth = mipWidth * 2;
            const u32 prevHeight = mipHeight * 2;
            const u8* prevData = mipData[level - 1].data();
            u8* currData = mipData[level].data();

            for (u32 y = 0; y < mipHeight; ++y) {
                for (u32 x = 0; x < mipWidth; ++x) {
                    // Sample 2x2 region from previous level
                    const u32 px0 = x * 2;
                    const u32 py0 = y * 2;
                    const u32 px1 = std::min(px0 + 1, prevWidth - 1);
                    const u32 py1 = std::min(py0 + 1, prevHeight - 1);

                    // Average the 2x2 pixels
                    for (u32 c = 0; c < bytesPerPixel; ++c) {
                        const u32 s0 = prevData[(py0 * prevWidth + px0) * bytesPerPixel + c];
                        const u32 s1 = prevData[(py0 * prevWidth + px1) * bytesPerPixel + c];
                        const u32 s2 = prevData[(py1 * prevWidth + px0) * bytesPerPixel + c];
                        const u32 s3 = prevData[(py1 * prevWidth + px1) * bytesPerPixel + c];

                        currData[(y * mipWidth + x) * bytesPerPixel + c] = static_cast<u8>((s0 + s1 + s2 + s3) / 4);
                    }
                }
            }
        }

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    // Upload all mip levels to GPU
    mipWidth = width;
    mipHeight = height;

    for (u32 level = 0; level < mipLevels; ++level) {
        const VkDeviceSize mipSize = mipWidth * mipHeight * bytesPerPixel;

        if (level > 0) {
            // Create staging buffer and upload
            VulkanBuffer stagingBuffer;
            stagingBuffer.Create(m_Context, mipSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            void* data = stagingBuffer.Map();
            memcpy(data, mipData[level].data(), static_cast<size_t>(mipSize));
            stagingBuffer.Unmap();

            // Copy to image
            VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

            VkBufferImageCopy region{};
            region.bufferOffset = 0;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = level;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount = 1;
            region.imageOffset = {0, 0, 0};
            region.imageExtent = {mipWidth, mipHeight, 1};

            vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.GetBuffer(), image,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            EndSingleTimeCommands(commandBuffer);
            stagingBuffer.Destroy();
        }

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    // Transition all mip levels to shader read-only
    TransitionImageLayout(image, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, mipLevels);

    (void)isSRGB; // Unused for now, but available for future sRGB-aware filtering
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
