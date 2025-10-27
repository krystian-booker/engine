#include "renderer/vulkan_depth.h"

#include "renderer/vulkan_context.h"
#include "renderer/vulkan_swapchain.h"

#include <stdexcept>
#include <vector>

namespace {

bool HasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

} // namespace

VulkanDepthBuffer::~VulkanDepthBuffer() {
    Shutdown();
}

void VulkanDepthBuffer::Init(VulkanContext* context, VulkanSwapchain* swapchain) {
    if (!context || !swapchain) {
        throw std::invalid_argument("VulkanDepthBuffer::Init requires valid context and swapchain");
    }

    Shutdown();

    m_Context = context;
    m_DepthFormat = FindDepthFormat();
    if (m_DepthFormat == VK_FORMAT_UNDEFINED) {
        throw std::runtime_error("VulkanDepthBuffer::Init failed to find supported depth format");
    }

    CreateDepthResources(swapchain);
}

void VulkanDepthBuffer::Shutdown() {
    DestroyResources();
    m_Context = nullptr;
    m_DepthFormat = VK_FORMAT_UNDEFINED;
}

void VulkanDepthBuffer::Recreate(VulkanSwapchain* swapchain) {
    if (!m_Context || !swapchain) {
        throw std::invalid_argument("VulkanDepthBuffer::Recreate requires initialized context and swapchain");
    }

    DestroyResources();
    CreateDepthResources(swapchain);
}

VkFormat VulkanDepthBuffer::FindDepthFormat() const {
    if (!m_Context) {
        return VK_FORMAT_UNDEFINED;
    }

    const std::vector<VkFormat> candidates = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };

    for (VkFormat format : candidates) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(m_Context->GetPhysicalDevice(), format, &properties);

        if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return format;
        }
    }

    return VK_FORMAT_UNDEFINED;
}

void VulkanDepthBuffer::CreateDepthResources(VulkanSwapchain* swapchain) {
    if (!m_Context) {
        throw std::runtime_error("VulkanDepthBuffer::CreateDepthResources requires initialized context");
    }

    const VkDevice device = m_Context->GetDevice();
    const VkExtent2D extent = swapchain->GetExtent();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = extent.width;
    imageInfo.extent.height = extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = m_DepthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &m_DepthImage) != VK_SUCCESS) {
        throw std::runtime_error("VulkanDepthBuffer::CreateDepthResources failed to create depth image");
    }

    VkMemoryRequirements memRequirements{};
    vkGetImageMemoryRequirements(device, m_DepthImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_DepthImageMemory) != VK_SUCCESS) {
        throw std::runtime_error("VulkanDepthBuffer::CreateDepthResources failed to allocate depth image memory");
    }

    if (vkBindImageMemory(device, m_DepthImage, m_DepthImageMemory, 0) != VK_SUCCESS) {
        throw std::runtime_error("VulkanDepthBuffer::CreateDepthResources failed to bind depth image memory");
    }

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_DepthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_DepthFormat;
    viewInfo.subresourceRange.aspectMask = HasStencilComponent(m_DepthFormat)
        ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
        : VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &m_DepthImageView) != VK_SUCCESS) {
        throw std::runtime_error("VulkanDepthBuffer::CreateDepthResources failed to create depth image view");
    }
}

u32 VulkanDepthBuffer::FindMemoryType(u32 typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProperties{};
    vkGetPhysicalDeviceMemoryProperties(m_Context->GetPhysicalDevice(), &memProperties);

    for (u32 i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("VulkanDepthBuffer::FindMemoryType failed to find suitable memory type");
}

void VulkanDepthBuffer::DestroyResources() {
    if (!m_Context) {
        m_DepthImage = VK_NULL_HANDLE;
        m_DepthImageMemory = VK_NULL_HANDLE;
        m_DepthImageView = VK_NULL_HANDLE;
        return;
    }

    const VkDevice device = m_Context->GetDevice();

    if (m_DepthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_DepthImageView, nullptr);
        m_DepthImageView = VK_NULL_HANDLE;
    }

    if (m_DepthImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_DepthImage, nullptr);
        m_DepthImage = VK_NULL_HANDLE;
    }

    if (m_DepthImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_DepthImageMemory, nullptr);
        m_DepthImageMemory = VK_NULL_HANDLE;
    }
}

