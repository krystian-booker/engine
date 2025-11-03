#include "vulkan_shadow_map.h"
#include "vulkan_context.h"
#include <stdexcept>
#include <array>

VulkanShadowMap::~VulkanShadowMap() {
    Destroy();
}

void VulkanShadowMap::CreateSingle(VulkanContext* context, u32 resolution) {
    if (!context) {
        throw std::invalid_argument("VulkanShadowMap::CreateSingle requires valid context");
    }

    Destroy();

    m_Context = context;
    m_Resolution = resolution;
    m_NumCascades = 1;
    m_IsCubemap = false;

    CreateDepthImage(resolution, 1);
    CreateImageViews();
    CreateRenderPass();
    CreateFramebuffers();
    CreateSampler();
}

void VulkanShadowMap::CreateCascaded(VulkanContext* context, u32 resolution, u32 numCascades) {
    if (!context) {
        throw std::invalid_argument("VulkanShadowMap::CreateCascaded requires valid context");
    }

    if (numCascades == 0 || numCascades > 8) {
        throw std::invalid_argument("VulkanShadowMap::CreateCascaded numCascades must be 1-8");
    }

    Destroy();

    m_Context = context;
    m_Resolution = resolution;
    m_NumCascades = numCascades;
    m_IsCubemap = false;

    CreateDepthImage(resolution, numCascades);
    CreateImageViews();
    CreateRenderPass();
    CreateFramebuffers();
    CreateSampler();
}

void VulkanShadowMap::CreateCubemap(VulkanContext* context, u32 resolution) {
    if (!context) {
        throw std::invalid_argument("VulkanShadowMap::CreateCubemap requires valid context");
    }

    Destroy();

    m_Context = context;
    m_Resolution = resolution;
    m_NumCascades = 6;  // 6 faces for cubemap
    m_IsCubemap = true;

    // Create cubemap depth image (6 faces)
    VkDevice device = m_Context->GetDevice();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = resolution;
    imageInfo.extent.height = resolution;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 6;
    imageInfo.format = m_DepthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;  // Required for cubemap

    if (vkCreateImage(device, &imageInfo, nullptr, &m_DepthImage) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create cubemap shadow map depth image");
    }

    // Allocate memory
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, m_DepthImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits,
                                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_DepthImageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate cubemap shadow map depth image memory");
    }

    vkBindImageMemory(device, m_DepthImage, m_DepthImageMemory, 0);

    CreateImageViews();
    CreateRenderPass();
    CreateFramebuffers();
    CreateSampler();
}

void VulkanShadowMap::Destroy() {
    DestroyResources();
    m_Context = nullptr;
    m_Resolution = 0;
    m_NumCascades = 1;
    m_IsCubemap = false;
}

VkImageView VulkanShadowMap::GetCascadeImageView(u32 cascade) const {
    if (cascade >= m_CascadeImageViews.size()) {
        return VK_NULL_HANDLE;
    }
    return m_CascadeImageViews[cascade];
}

VkFramebuffer VulkanShadowMap::GetFramebuffer(u32 cascade) const {
    if (cascade >= m_Framebuffers.size()) {
        return VK_NULL_HANDLE;
    }
    return m_Framebuffers[cascade];
}

void VulkanShadowMap::CreateDepthImage(u32 resolution, u32 arrayLayers) {
    VkDevice device = m_Context->GetDevice();

    // Create depth image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = resolution;
    imageInfo.extent.height = resolution;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = arrayLayers;
    imageInfo.format = m_DepthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    if (vkCreateImage(device, &imageInfo, nullptr, &m_DepthImage) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow map depth image");
    }

    // Allocate memory
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, m_DepthImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits,
                                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_DepthImageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate shadow map depth image memory");
    }

    vkBindImageMemory(device, m_DepthImage, m_DepthImageMemory, 0);
}

void VulkanShadowMap::CreateImageViews() {
    VkDevice device = m_Context->GetDevice();

    // Create full array/cubemap view for sampling in shaders
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_DepthImage;

    if (m_IsCubemap) {
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    } else if (m_NumCascades > 1) {
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    } else {
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    }

    viewInfo.format = m_DepthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = m_NumCascades;

    if (vkCreateImageView(device, &viewInfo, nullptr, &m_DepthImageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow map image view");
    }

    // Create individual layer views for framebuffer attachments
    m_CascadeImageViews.resize(m_NumCascades);
    for (u32 i = 0; i < m_NumCascades; ++i) {
        VkImageViewCreateInfo layerViewInfo{};
        layerViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        layerViewInfo.image = m_DepthImage;
        layerViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        layerViewInfo.format = m_DepthFormat;
        layerViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        layerViewInfo.subresourceRange.baseMipLevel = 0;
        layerViewInfo.subresourceRange.levelCount = 1;
        layerViewInfo.subresourceRange.baseArrayLayer = i;
        layerViewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &layerViewInfo, nullptr, &m_CascadeImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shadow map cascade image view");
        }
    }
}

void VulkanShadowMap::CreateRenderPass() {
    VkDevice device = m_Context->GetDevice();

    // Depth attachment
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = m_DepthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 0;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Subpass
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 0;  // Depth-only pass
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    // Subpass dependency for layout transitions
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &depthAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow map render pass");
    }
}

void VulkanShadowMap::CreateFramebuffers() {
    VkDevice device = m_Context->GetDevice();

    m_Framebuffers.resize(m_NumCascades);

    for (u32 i = 0; i < m_NumCascades; ++i) {
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_RenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &m_CascadeImageViews[i];
        framebufferInfo.width = m_Resolution;
        framebufferInfo.height = m_Resolution;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &m_Framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shadow map framebuffer");
        }
    }
}

void VulkanShadowMap::CreateSampler() {
    VkDevice device = m_Context->GetDevice();

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;  // Outside shadow = no shadow
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.compareEnable = VK_TRUE;  // Enable hardware PCF
    samplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &m_Sampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow map sampler");
    }
}

void VulkanShadowMap::DestroyResources() {
    if (!m_Context) {
        return;
    }

    VkDevice device = m_Context->GetDevice();

    for (auto framebuffer : m_Framebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
    }
    m_Framebuffers.clear();

    if (m_RenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_RenderPass, nullptr);
        m_RenderPass = VK_NULL_HANDLE;
    }

    if (m_Sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_Sampler, nullptr);
        m_Sampler = VK_NULL_HANDLE;
    }

    for (auto imageView : m_CascadeImageViews) {
        if (imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, imageView, nullptr);
        }
    }
    m_CascadeImageViews.clear();

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

u32 VulkanShadowMap::FindMemoryType(u32 typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDevice physicalDevice = m_Context->GetPhysicalDevice();

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (u32 i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type for shadow map");
}
