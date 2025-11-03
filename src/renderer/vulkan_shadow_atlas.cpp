#include "vulkan_shadow_atlas.h"
#include "vulkan_context.h"
#include <stdexcept>
#include <cmath>

VulkanShadowAtlas::~VulkanShadowAtlas() {
    Destroy();
}

void VulkanShadowAtlas::Init(VulkanContext* context, const ShadowAtlasConfig& config) {
    if (!context) {
        throw std::invalid_argument("VulkanShadowAtlas::Init requires valid context");
    }

    if (config.atlasSize == 0 || (config.atlasSize & (config.atlasSize - 1)) != 0) {
        throw std::invalid_argument("VulkanShadowAtlas::Init atlasSize must be power of 2");
    }

    if (config.numArrayLayers == 0 || config.numArrayLayers > 16) {
        throw std::invalid_argument("VulkanShadowAtlas::Init numArrayLayers must be 1-16");
    }

    Destroy();

    m_Context = context;
    m_Config = config;

    // Initialize pages
    m_Pages.resize(config.numArrayLayers);
    for (auto& page : m_Pages) {
        page.freeSpace = config.atlasSize * config.atlasSize;
        // Start with one large free node per page
        AllocationNode rootNode;
        rootNode.x = 0;
        rootNode.y = 0;
        rootNode.width = config.atlasSize;
        rootNode.height = config.atlasSize;
        rootNode.layer = 0;  // Will be set per page
        rootNode.isFree = true;
        page.nodes.push_back(rootNode);
    }

    CreateDepthImage();
    CreateImageViews();
    CreateRenderPass();
    CreateFramebuffers();
    CreateSampler();
}

void VulkanShadowAtlas::Destroy() {
    DestroyResources();

    m_Pages.clear();
    m_AllocationTable.clear();
    m_Regions.clear();
    m_FreeHandles.clear();
    m_TotalAllocations = 0;
    m_TotalMemoryUsed = 0;
    m_Context = nullptr;
}

ShadowAtlasHandle VulkanShadowAtlas::Allocate(u32 resolution) {
    if (!m_Context) {
        return ShadowAtlasHandle{};
    }

    // Clamp resolution to valid range
    resolution = std::max(m_Config.minAllocationSize, std::min(resolution, m_Config.maxAllocationSize));

    // Round up to next power of 2 for better packing
    u32 allocSize = 1;
    while (allocSize < resolution) {
        allocSize *= 2;
    }

    if (allocSize > m_Config.atlasSize) {
        return ShadowAtlasHandle{};  // Too large to fit
    }

    // Try to allocate in existing pages
    for (u32 pageIdx = 0; pageIdx < m_Pages.size(); ++pageIdx) {
        std::optional<u32> nodeIdx = AllocateInPage(pageIdx, allocSize);
        if (nodeIdx.has_value()) {
            // Successfully allocated
            AllocationNode& node = m_Pages[pageIdx].nodes[*nodeIdx];

            // Create region info
            ShadowAtlasRegion region;
            region.x = node.x;
            region.y = node.y;
            region.width = allocSize;
            region.height = allocSize;
            region.arrayLayer = pageIdx;
            region.isValid = true;

            // Calculate normalized UV coordinates
            region.uvOffsetX = static_cast<f32>(node.x) / static_cast<f32>(m_Config.atlasSize);
            region.uvOffsetY = static_cast<f32>(node.y) / static_cast<f32>(m_Config.atlasSize);
            region.uvScaleX = static_cast<f32>(allocSize) / static_cast<f32>(m_Config.atlasSize);
            region.uvScaleY = static_cast<f32>(allocSize) / static_cast<f32>(m_Config.atlasSize);

            // Get or create handle
            ShadowAtlasHandle handle;
            if (!m_FreeHandles.empty()) {
                handle.index = m_FreeHandles.back();
                m_FreeHandles.pop_back();
                m_AllocationTable[handle.index].generation++;
                m_Regions[handle.index] = region;
            } else {
                handle.index = static_cast<u32>(m_AllocationTable.size());
                AllocationNode allocNode = node;
                allocNode.isFree = false;
                allocNode.generation = 0;
                m_AllocationTable.push_back(allocNode);
                m_Regions.push_back(region);
            }

            handle.generation = m_AllocationTable[handle.index].generation;

            // Update statistics
            m_TotalAllocations++;
            m_TotalMemoryUsed += allocSize * allocSize;
            m_Pages[pageIdx].freeSpace -= allocSize * allocSize;

            return handle;
        }
    }

    // Could not allocate
    return ShadowAtlasHandle{};
}

void VulkanShadowAtlas::Free(ShadowAtlasHandle handle) {
    if (!handle.IsValid() || handle.index >= m_AllocationTable.size()) {
        return;
    }

    AllocationNode& allocation = m_AllocationTable[handle.index];
    if (allocation.generation != handle.generation || allocation.isFree) {
        return;  // Invalid or already freed
    }

    // Mark as free
    allocation.isFree = true;
    m_Regions[handle.index].isValid = false;

    // Update statistics
    u32 allocSize = allocation.width * allocation.height;
    m_TotalMemoryUsed -= allocSize;
    m_TotalAllocations--;

    // Find the node in the page and mark it free
    u32 pageIdx = allocation.layer;
    if (pageIdx < m_Pages.size()) {
        auto& page = m_Pages[pageIdx];
        for (auto& node : page.nodes) {
            if (node.x == allocation.x && node.y == allocation.y &&
                node.width == allocation.width && node.height == allocation.height && !node.isFree) {
                node.isFree = true;
                page.freeSpace += allocSize;
                break;
            }
        }
    }

    // Add handle to free list for reuse
    m_FreeHandles.push_back(handle.index);
}

const ShadowAtlasRegion* VulkanShadowAtlas::GetRegion(ShadowAtlasHandle handle) const {
    if (!handle.IsValid() || handle.index >= m_Regions.size()) {
        return nullptr;
    }

    const auto& allocation = m_AllocationTable[handle.index];
    if (allocation.generation != handle.generation || allocation.isFree) {
        return nullptr;
    }

    return &m_Regions[handle.index];
}

void VulkanShadowAtlas::ClearAllocations() {
    for (auto& page : m_Pages) {
        page.nodes.clear();
        page.freeSpace = m_Config.atlasSize * m_Config.atlasSize;

        // Reset to single large free node
        AllocationNode rootNode;
        rootNode.x = 0;
        rootNode.y = 0;
        rootNode.width = m_Config.atlasSize;
        rootNode.height = m_Config.atlasSize;
        rootNode.isFree = true;
        page.nodes.push_back(rootNode);
    }

    m_AllocationTable.clear();
    m_Regions.clear();
    m_FreeHandles.clear();
    m_TotalAllocations = 0;
    m_TotalMemoryUsed = 0;
}

VkImageView VulkanShadowAtlas::GetLayerImageView(u32 layer) const {
    if (layer >= m_LayerImageViews.size()) {
        return VK_NULL_HANDLE;
    }
    return m_LayerImageViews[layer];
}

VkFramebuffer VulkanShadowAtlas::GetFramebuffer(u32 layer) const {
    if (layer >= m_Framebuffers.size()) {
        return VK_NULL_HANDLE;
    }
    return m_Framebuffers[layer];
}

f32 VulkanShadowAtlas::GetFragmentation() const {
    if (m_Pages.empty()) {
        return 0.0f;
    }

    u32 totalPages = static_cast<u32>(m_Pages.size());
    u32 totalCapacity = totalPages * m_Config.atlasSize * m_Config.atlasSize;
    u32 totalFree = 0;

    for (const auto& page : m_Pages) {
        totalFree += page.freeSpace;
    }

    u32 used = totalCapacity - totalFree;
    if (used == 0) {
        return 0.0f;
    }

    // Calculate number of free fragments
    u32 freeFragments = 0;
    for (const auto& page : m_Pages) {
        for (const auto& node : page.nodes) {
            if (node.isFree) {
                freeFragments++;
            }
        }
    }

    // More fragments = more fragmentation
    // Normalize by number of pages
    return static_cast<f32>(freeFragments) / static_cast<f32>(totalPages * 10);
}

std::optional<u32> VulkanShadowAtlas::AllocateInPage(u32 pageIndex, u32 resolution) {
    if (pageIndex >= m_Pages.size()) {
        return std::nullopt;
    }

    auto& page = m_Pages[pageIndex];

    // Check if page has enough free space
    if (page.freeSpace < resolution * resolution) {
        return std::nullopt;
    }

    // Try to find a free node that fits
    for (u32 i = 0; i < page.nodes.size(); ++i) {
        auto& node = page.nodes[i];
        if (node.isFree && node.width >= resolution && node.height >= resolution) {
            u32 allocX, allocY;
            if (TryAllocateInNode(node, resolution, allocX, allocY)) {
                // Mark node as allocated
                node.isFree = false;
                node.layer = pageIndex;

                // If node is larger than needed, split it
                if (node.width > resolution || node.height > resolution) {
                    SplitNode(pageIndex, i, allocX, allocY, resolution);
                }

                return i;
            }
        }
    }

    return std::nullopt;
}

bool VulkanShadowAtlas::TryAllocateInNode(AllocationNode& node, u32 resolution, u32& outX, u32& outY) {
    if (!node.isFree || node.width < resolution || node.height < resolution) {
        return false;
    }

    // Simple allocation at node origin
    outX = node.x;
    outY = node.y;
    return true;
}

void VulkanShadowAtlas::SplitNode(u32 pageIndex, u32 nodeIndex, u32 allocX, u32 allocY, u32 allocSize) {
    auto& page = m_Pages[pageIndex];
    auto& node = page.nodes[nodeIndex];

    // Split the node into smaller pieces
    // We use a simple guillotine split: create right and bottom remainder nodes

    u32 remainderRight = node.width - allocSize;
    u32 remainderBottom = node.height - allocSize;

    // Shrink the allocated node to exact size
    node.width = allocSize;
    node.height = allocSize;

    // Create right remainder node if exists
    if (remainderRight > 0) {
        AllocationNode rightNode;
        rightNode.x = allocX + allocSize;
        rightNode.y = allocY;
        rightNode.width = remainderRight;
        rightNode.height = allocSize;
        rightNode.layer = pageIndex;
        rightNode.isFree = true;
        page.nodes.push_back(rightNode);
    }

    // Create bottom remainder node if exists
    if (remainderBottom > 0) {
        AllocationNode bottomNode;
        bottomNode.x = allocX;
        bottomNode.y = allocY + allocSize;
        bottomNode.width = node.width + remainderRight;  // Full width including right remainder
        bottomNode.height = remainderBottom;
        bottomNode.layer = pageIndex;
        bottomNode.isFree = true;
        page.nodes.push_back(bottomNode);
    }
}

void VulkanShadowAtlas::CreateDepthImage() {
    VkDevice device = m_Context->GetDevice();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = m_Config.atlasSize;
    imageInfo.extent.height = m_Config.atlasSize;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = m_Config.numArrayLayers;
    imageInfo.format = m_Config.depthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    if (vkCreateImage(device, &imageInfo, nullptr, &m_DepthImage) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow atlas depth image");
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
        throw std::runtime_error("Failed to allocate shadow atlas depth image memory");
    }

    vkBindImageMemory(device, m_DepthImage, m_DepthImageMemory, 0);
}

void VulkanShadowAtlas::CreateImageViews() {
    VkDevice device = m_Context->GetDevice();

    // Create full array view for sampling in shaders
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_DepthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewInfo.format = m_Config.depthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = m_Config.numArrayLayers;

    if (vkCreateImageView(device, &viewInfo, nullptr, &m_DepthImageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow atlas image view");
    }

    // Create individual layer views for rendering
    m_LayerImageViews.resize(m_Config.numArrayLayers);
    for (u32 i = 0; i < m_Config.numArrayLayers; ++i) {
        VkImageViewCreateInfo layerViewInfo{};
        layerViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        layerViewInfo.image = m_DepthImage;
        layerViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        layerViewInfo.format = m_Config.depthFormat;
        layerViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        layerViewInfo.subresourceRange.baseMipLevel = 0;
        layerViewInfo.subresourceRange.levelCount = 1;
        layerViewInfo.subresourceRange.baseArrayLayer = i;
        layerViewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &layerViewInfo, nullptr, &m_LayerImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shadow atlas layer image view");
        }
    }
}

void VulkanShadowAtlas::CreateRenderPass() {
    VkDevice device = m_Context->GetDevice();

    // Depth attachment
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = m_Config.depthFormat;
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
        throw std::runtime_error("Failed to create shadow atlas render pass");
    }
}

void VulkanShadowAtlas::CreateFramebuffers() {
    VkDevice device = m_Context->GetDevice();

    m_Framebuffers.resize(m_Config.numArrayLayers);

    for (u32 i = 0; i < m_Config.numArrayLayers; ++i) {
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_RenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &m_LayerImageViews[i];
        framebufferInfo.width = m_Config.atlasSize;
        framebufferInfo.height = m_Config.atlasSize;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &m_Framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shadow atlas framebuffer");
        }
    }
}

void VulkanShadowAtlas::CreateSampler() {
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
        throw std::runtime_error("Failed to create shadow atlas sampler");
    }
}

void VulkanShadowAtlas::DestroyResources() {
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

    for (auto imageView : m_LayerImageViews) {
        if (imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, imageView, nullptr);
        }
    }
    m_LayerImageViews.clear();

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

u32 VulkanShadowAtlas::FindMemoryType(u32 typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDevice physicalDevice = m_Context->GetPhysicalDevice();

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (u32 i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type for shadow atlas");
}
