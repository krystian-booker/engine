#include "renderer/vulkan_render_target.h"

#include "renderer/vulkan_context.h"

#include <stdexcept>
#include <array>

namespace {

bool HasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

} // namespace

VulkanRenderTarget::~VulkanRenderTarget() {
    Destroy();
}

void VulkanRenderTarget::Create(VulkanContext* context, u32 width, u32 height) {
    if (!context) {
        throw std::invalid_argument("VulkanRenderTarget::Create requires valid context");
    }
    if (width == 0 || height == 0) {
        throw std::invalid_argument("VulkanRenderTarget::Create requires non-zero dimensions");
    }

    Destroy();

    m_Context = context;
    m_Width = width;
    m_Height = height;
    m_DepthFormat = FindDepthFormat();

    if (m_DepthFormat == VK_FORMAT_UNDEFINED) {
        throw std::runtime_error("VulkanRenderTarget::Create failed to find supported depth format");
    }

    CreateColorResources();
    CreateDepthResources();
    CreateRenderPass();
    CreateFramebuffer();
    CreateSampler();
}

void VulkanRenderTarget::Resize(u32 width, u32 height) {
    if (width == 0 || height == 0) {
        throw std::invalid_argument("VulkanRenderTarget::Resize requires non-zero dimensions");
    }
    if (!m_Context) {
        throw std::runtime_error("VulkanRenderTarget::Resize called on uninitialized render target");
    }

    // Wait for device to finish using resources
    vkDeviceWaitIdle(m_Context->GetDevice());

    DestroyResources();

    m_Width = width;
    m_Height = height;

    CreateColorResources();
    CreateDepthResources();
    CreateFramebuffer();
    // Render pass and sampler don't need recreation
}

void VulkanRenderTarget::Destroy() {
    if (!m_Context) {
        return;
    }

    vkDeviceWaitIdle(m_Context->GetDevice());
    DestroyResources();

    const VkDevice device = m_Context->GetDevice();

    if (m_RenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_RenderPass, nullptr);
        m_RenderPass = VK_NULL_HANDLE;
    }

    if (m_ColorSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_ColorSampler, nullptr);
        m_ColorSampler = VK_NULL_HANDLE;
    }

    m_Context = nullptr;
    m_Width = 0;
    m_Height = 0;
}

void VulkanRenderTarget::CreateColorResources() {
    const VkDevice device = m_Context->GetDevice();

    // Create color image (HDR format)
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = m_Width;
    imageInfo.extent.height = m_Height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = m_ColorFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &m_ColorImage) != VK_SUCCESS) {
        throw std::runtime_error("VulkanRenderTarget::CreateColorResources failed to create color image");
    }

    VkMemoryRequirements memRequirements{};
    vkGetImageMemoryRequirements(device, m_ColorImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_ColorImageMemory) != VK_SUCCESS) {
        throw std::runtime_error("VulkanRenderTarget::CreateColorResources failed to allocate color image memory");
    }

    if (vkBindImageMemory(device, m_ColorImage, m_ColorImageMemory, 0) != VK_SUCCESS) {
        throw std::runtime_error("VulkanRenderTarget::CreateColorResources failed to bind color image memory");
    }

    // Create color image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_ColorImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_ColorFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &m_ColorImageView) != VK_SUCCESS) {
        throw std::runtime_error("VulkanRenderTarget::CreateColorResources failed to create color image view");
    }
}

void VulkanRenderTarget::CreateDepthResources() {
    const VkDevice device = m_Context->GetDevice();

    // Create depth image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = m_Width;
    imageInfo.extent.height = m_Height;
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
        throw std::runtime_error("VulkanRenderTarget::CreateDepthResources failed to create depth image");
    }

    VkMemoryRequirements memRequirements{};
    vkGetImageMemoryRequirements(device, m_DepthImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_DepthImageMemory) != VK_SUCCESS) {
        throw std::runtime_error("VulkanRenderTarget::CreateDepthResources failed to allocate depth image memory");
    }

    if (vkBindImageMemory(device, m_DepthImage, m_DepthImageMemory, 0) != VK_SUCCESS) {
        throw std::runtime_error("VulkanRenderTarget::CreateDepthResources failed to bind depth image memory");
    }

    // Create depth image view
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
        throw std::runtime_error("VulkanRenderTarget::CreateDepthResources failed to create depth image view");
    }
}

void VulkanRenderTarget::CreateRenderPass() {
    const VkDevice device = m_Context->GetDevice();

    // Color attachment (HDR format, will be read by ImGui)
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_ColorFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;  // For ImGui sampling

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Depth attachment
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = m_DepthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Subpass
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    // Subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 2> dependencies{};

    // External -> Subpass 0 (for color and depth writes)
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = 0;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // Subpass 0 -> External (for shader reading in ImGui)
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    const std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<u32>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = static_cast<u32>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS) {
        throw std::runtime_error("VulkanRenderTarget::CreateRenderPass failed to create render pass");
    }
}

void VulkanRenderTarget::CreateFramebuffer() {
    const VkDevice device = m_Context->GetDevice();

    std::array<VkImageView, 2> attachments = {
        m_ColorImageView,
        m_DepthImageView
    };

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_RenderPass;
    framebufferInfo.attachmentCount = static_cast<u32>(attachments.size());
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = m_Width;
    framebufferInfo.height = m_Height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &m_Framebuffer) != VK_SUCCESS) {
        throw std::runtime_error("VulkanRenderTarget::CreateFramebuffer failed to create framebuffer");
    }
}

void VulkanRenderTarget::CreateSampler() {
    const VkDevice device = m_Context->GetDevice();

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &m_ColorSampler) != VK_SUCCESS) {
        throw std::runtime_error("VulkanRenderTarget::CreateSampler failed to create sampler");
    }
}

void VulkanRenderTarget::DestroyResources() {
    if (!m_Context) {
        return;
    }

    const VkDevice device = m_Context->GetDevice();

    if (m_Framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, m_Framebuffer, nullptr);
        m_Framebuffer = VK_NULL_HANDLE;
    }

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

    if (m_ColorImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_ColorImageView, nullptr);
        m_ColorImageView = VK_NULL_HANDLE;
    }

    if (m_ColorImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_ColorImage, nullptr);
        m_ColorImage = VK_NULL_HANDLE;
    }

    if (m_ColorImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_ColorImageMemory, nullptr);
        m_ColorImageMemory = VK_NULL_HANDLE;
    }
}

VkFormat VulkanRenderTarget::FindDepthFormat() const {
    if (!m_Context) {
        return VK_FORMAT_UNDEFINED;
    }

    const std::array<VkFormat, 3> candidates = {
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

u32 VulkanRenderTarget::FindMemoryType(u32 typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProperties{};
    vkGetPhysicalDeviceMemoryProperties(m_Context->GetPhysicalDevice(), &memProperties);

    for (u32 i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("VulkanRenderTarget::FindMemoryType failed to find suitable memory type");
}
