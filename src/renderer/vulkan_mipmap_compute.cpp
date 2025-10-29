#include "renderer/vulkan_mipmap_compute.h"

#include "renderer/vulkan_context.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace {

std::vector<char> ReadBinaryFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open shader file: " + path.string());
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(static_cast<size_t>(size));
    if (!file.read(buffer.data(), size)) {
        throw std::runtime_error("Failed to read shader file: " + path.string());
    }

    return buffer;
}

struct MipgenPushConstants {
    u32 SrcWidth;
    u32 SrcHeight;
    u32 DstWidth;
    u32 DstHeight;
    u32 SrcLevel;
    u32 Options;
};
static_assert(sizeof(MipgenPushConstants) == 24, "Push constant size mismatch");

constexpr u32 kColorPremultipliedAlphaFlag = 0x1u;
constexpr u32 kRoughnessHasNormalsFlag = 0x1u;

// RAII helper for command buffers to prevent leaks on early exit
class ScopedCommandBuffer {
public:
    ScopedCommandBuffer(VkDevice device, VkCommandPool pool)
        : m_Device(device), m_Pool(pool) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = pool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(device, &allocInfo, &m_CommandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffer for mipmap compute");
        }
    }

    ~ScopedCommandBuffer() {
        if (m_CommandBuffer != VK_NULL_HANDLE) {
            vkFreeCommandBuffers(m_Device, m_Pool, 1, &m_CommandBuffer);
        }
    }

    ScopedCommandBuffer(const ScopedCommandBuffer&) = delete;
    ScopedCommandBuffer& operator=(const ScopedCommandBuffer&) = delete;

    VkCommandBuffer Get() const { return m_CommandBuffer; }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    VkCommandPool m_Pool = VK_NULL_HANDLE;
    VkCommandBuffer m_CommandBuffer = VK_NULL_HANDLE;
};

// RAII helper for descriptor sets to prevent leaks on early exit
class ScopedDescriptorSets {
public:
    ScopedDescriptorSets(VkDevice device, VkDescriptorPool pool)
        : m_Device(device), m_Pool(pool) {}

    ~ScopedDescriptorSets() {
        if (!m_Sets.empty()) {
            vkFreeDescriptorSets(m_Device, m_Pool, static_cast<u32>(m_Sets.size()), m_Sets.data());
        }
    }

    ScopedDescriptorSets(const ScopedDescriptorSets&) = delete;
    ScopedDescriptorSets& operator=(const ScopedDescriptorSets&) = delete;

    void Add(VkDescriptorSet set) {
        m_Sets.push_back(set);
    }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    VkDescriptorPool m_Pool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_Sets;
};

} // namespace

VulkanMipmapCompute::~VulkanMipmapCompute() {
    Shutdown();
}

void VulkanMipmapCompute::Initialize(VulkanContext* context) {
    if (!context) {
        throw std::invalid_argument("VulkanMipmapCompute::Initialize requires a valid context");
    }

    if (m_Context == context) {
        return; // Already initialized for this context
    }

    Shutdown();

    m_Context = context;

    CreateDescriptorSetLayout();
    CreatePipelineLayout();
    CreateDescriptorPool();
    CreatePipelines();
}

void VulkanMipmapCompute::Shutdown() {
    if (!m_Context) {
        return;
    }

    VkDevice device = m_Context->GetDevice();

    for (VkPipeline& pipeline : m_Pipelines) {
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, pipeline, nullptr);
            pipeline = VK_NULL_HANDLE;
        }
    }

    if (m_DescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
        m_DescriptorPool = VK_NULL_HANDLE;
    }

    if (m_PipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
        m_PipelineLayout = VK_NULL_HANDLE;
    }

    if (m_DescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_DescriptorSetLayout, nullptr);
        m_DescriptorSetLayout = VK_NULL_HANDLE;
    }

    m_Context = nullptr;
}

void VulkanMipmapCompute::Generate(const Params& params) {
    // Thread safety: Lock for the entire generation process
    // This protects descriptor pool allocation and queue submission
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (!m_Context) {
        throw std::runtime_error("VulkanMipmapCompute::Generate called before Initialize");
    }

    if (params.image == VK_NULL_HANDLE || params.mipLevels < 2) {
        return; // Nothing to do
    }

    // Validation: Check format is defined
    if (params.format == VK_FORMAT_UNDEFINED) {
        throw std::invalid_argument("VulkanMipmapCompute::Generate: format is VK_FORMAT_UNDEFINED");
    }

    // Validation: Check dimensions are valid
    if (params.width == 0 || params.height == 0) {
        throw std::invalid_argument("VulkanMipmapCompute::Generate: width and height must be non-zero");
    }

    // Validation: Check mip levels are reasonable
    const u32 maxPossibleMips = static_cast<u32>(std::floor(std::log2(std::max(params.width, params.height)))) + 1;
    if (params.mipLevels > maxPossibleMips) {
        throw std::invalid_argument("VulkanMipmapCompute::Generate: mipLevels (" + std::to_string(params.mipLevels) +
                                    ") exceeds maximum possible mips (" + std::to_string(maxPossibleMips) +
                                    ") for " + std::to_string(params.width) + "x" + std::to_string(params.height));
    }

    // Validation: Check layer count
    if (params.layerCount == 0) {
        throw std::invalid_argument("VulkanMipmapCompute::Generate: layerCount must be at least 1");
    }

    VkDevice device = m_Context->GetDevice();
    VkCommandPool commandPool = m_Context->GetCommandPool();

    // Use RAII wrapper for command buffer to prevent leaks on exceptions
    ScopedCommandBuffer scopedCmd(device, commandPool);
    VkCommandBuffer cmd = scopedCmd.Get();

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin command buffer for mipmap compute");
    }

    VkFormat storageFormat = GetStorageCompatibleFormat(params.format);
    if (storageFormat == VK_FORMAT_UNDEFINED) {
        throw std::runtime_error("VulkanMipmapCompute::Generate: format " + std::to_string(static_cast<u32>(params.format)) +
                                " has no storage-compatible equivalent");
    }

    // Validation: Check if the storage format supports storage image usage
    if (!m_Context->SupportsStorageImage(storageFormat)) {
        throw std::runtime_error("VulkanMipmapCompute::Generate: storage format " + std::to_string(static_cast<u32>(storageFormat)) +
                                " (derived from " + std::to_string(static_cast<u32>(params.format)) +
                                ") does not support VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT. Image must be created with VK_IMAGE_USAGE_STORAGE_BIT.");
    }

    VkFormat sampledFormat = (params.variant == Variant::Srgb)
                                 ? storageFormat
                                 : params.format;

    const VkImage normalImage = params.hasNormalMap ? params.normalImage : VK_NULL_HANDLE;
    const VkFormat normalFormat = params.hasNormalMap ? params.normalFormat : VK_FORMAT_UNDEFINED;

    std::vector<VkImageView> temporaryViews;

    // Use RAII wrapper for descriptor sets to prevent leaks on exceptions
    ScopedDescriptorSets scopedDescriptorSets(device, m_DescriptorPool);

    auto trackView = [&temporaryViews](VkImageView view) {
        if (view != VK_NULL_HANDLE) {
            temporaryViews.push_back(view);
        }
    };

    const u32 lastMip = params.mipLevels - 1;
    VkPipeline pipeline = GetPipeline(params.variant);
    if (pipeline == VK_NULL_HANDLE) {
        throw std::runtime_error("Missing compute pipeline for mipmap generation");
    }

    for (u32 layer = 0; layer < params.layerCount; ++layer) {
        const u32 arrayLayer = params.baseArrayLayer + layer;

        for (u32 mip = 1; mip <= lastMip; ++mip) {
            const u32 srcMip = mip - 1;

            const u32 srcWidth = std::max(1u, params.width >> srcMip);
            const u32 srcHeight = std::max(1u, params.height >> srcMip);
            const u32 dstWidth = std::max(1u, params.width >> mip);
            const u32 dstHeight = std::max(1u, params.height >> mip);

            VkImageView srcView = CreateImageView(params.image, sampledFormat, srcMip, arrayLayer);
            trackView(srcView);

            VkImageView dstView = CreateStorageView(params.image, storageFormat, mip, arrayLayer);
            trackView(dstView);

            VkImageView normalView = VK_NULL_HANDLE;
            if (params.variant == Variant::Roughness) {
                if (normalImage != VK_NULL_HANDLE) {
                    if (normalImage == params.image && normalFormat == params.format) {
                        normalView = srcView;
                    } else {
                        VkFormat normalSampleFormat = (params.variant == Variant::Roughness)
                                                          ? GetStorageCompatibleFormat(normalFormat)
                                                          : normalFormat;
                        normalView = CreateImageView(normalImage, normalSampleFormat, srcMip, arrayLayer);
                        trackView(normalView);
                    }
                } else {
                    normalView = srcView;
                }
            } else {
                normalView = srcView;
            }

            VkImageMemoryBarrier barriers[2]{};

            barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barriers[0].image = params.image;
            barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barriers[0].subresourceRange.baseMipLevel = srcMip;
            barriers[0].subresourceRange.levelCount = 1;
            barriers[0].subresourceRange.baseArrayLayer = arrayLayer;
            barriers[0].subresourceRange.layerCount = 1;
            barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[0].oldLayout = (mip == 1) ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barriers[0].srcAccessMask = (mip == 1) ? VK_ACCESS_TRANSFER_WRITE_BIT : VK_ACCESS_SHADER_WRITE_BIT;
            barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barriers[1].image = params.image;
            barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barriers[1].subresourceRange.baseMipLevel = mip;
            barriers[1].subresourceRange.levelCount = 1;
            barriers[1].subresourceRange.baseArrayLayer = arrayLayer;
            barriers[1].subresourceRange.layerCount = 1;
            barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            // Mip levels 1+ are in UNDEFINED layout after image creation (only mip 0 is uploaded)
            barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barriers[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barriers[1].srcAccessMask = 0;
            barriers[1].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

            VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            if (mip > 1) {
                srcStage |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            }

            vkCmdPipelineBarrier(cmd,
                                 srcStage,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 0,
                                 0, nullptr,
                                 0, nullptr,
                                 static_cast<u32>(std::size(barriers)), barriers);

            VkDescriptorSetAllocateInfo setAlloc{};
            setAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            setAlloc.descriptorPool = m_DescriptorPool;
            setAlloc.descriptorSetCount = 1;
            setAlloc.pSetLayouts = &m_DescriptorSetLayout;

            VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
            if (vkAllocateDescriptorSets(device, &setAlloc, &descriptorSet) != VK_SUCCESS) {
                throw std::runtime_error("Failed to allocate descriptor set for mipmap compute");
            }
            scopedDescriptorSets.Add(descriptorSet);

            VkDescriptorImageInfo srcImageInfo{};
            srcImageInfo.imageView = srcView;
            srcImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkDescriptorImageInfo dstImageInfo{};
            dstImageInfo.imageView = dstView;
            dstImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            VkDescriptorImageInfo normalImageInfo{};
            normalImageInfo.imageView = normalView;
            normalImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            std::array<VkWriteDescriptorSet, 3> writes{};

            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = descriptorSet;
            writes[0].dstBinding = 0;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            writes[0].descriptorCount = 1;
            writes[0].pImageInfo = &srcImageInfo;

            writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet = descriptorSet;
            writes[1].dstBinding = 1;
            writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writes[1].descriptorCount = 1;
            writes[1].pImageInfo = &dstImageInfo;

            writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[2].dstSet = descriptorSet;
            writes[2].dstBinding = 2;
            writes[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            writes[2].descriptorCount = 1;
            writes[2].pImageInfo = &normalImageInfo;

            vkUpdateDescriptorSets(device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_PipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

            MipgenPushConstants push{};
            push.SrcWidth = srcWidth;
            push.SrcHeight = srcHeight;
            push.DstWidth = dstWidth;
            push.DstHeight = dstHeight;
            push.SrcLevel = 0;
            push.Options = 0;

            switch (params.variant) {
                case Variant::Color:
                    // Set premultiplied alpha flag if requested
                    push.Options = (params.alphaMode == AlphaMode::Premultiplied) ? kColorPremultipliedAlphaFlag : 0;
                    break;
                case Variant::Normal:
                case Variant::Srgb:
                    push.Options = 0;
                    break;
                case Variant::Roughness:
                    push.Options = params.hasNormalMap ? kRoughnessHasNormalsFlag : 0;
                    break;
                default:
                    break;
            }

            vkCmdPushConstants(cmd, m_PipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MipgenPushConstants), &push);

            const u32 groupX = (dstWidth + 7) / 8;
            const u32 groupY = (dstHeight + 7) / 8;
            vkCmdDispatch(cmd, groupX, groupY, 1);

            VkImageMemoryBarrier postBarrier{};
            postBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            postBarrier.image = params.image;
            postBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            postBarrier.subresourceRange.baseMipLevel = mip;
            postBarrier.subresourceRange.levelCount = 1;
            postBarrier.subresourceRange.baseArrayLayer = arrayLayer;
            postBarrier.subresourceRange.layerCount = 1;
            postBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            postBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            postBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            postBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            postBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            postBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(cmd,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 0,
                                 0, nullptr,
                                 0, nullptr,
                                 1, &postBarrier);
        }
    }

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record mipmap compute command buffer");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    VkQueue queue = m_Context->GetGraphicsQueue();
    if (vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit mipmap compute command buffer");
    }

    vkQueueWaitIdle(queue);

    // RAII wrappers (ScopedCommandBuffer and ScopedDescriptorSets) automatically clean up here
    // Image views still need manual cleanup as they're not RAII-wrapped yet
    for (VkImageView view : temporaryViews) {
        vkDestroyImageView(device, view, nullptr);
    }
}

void VulkanMipmapCompute::CreateDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding srcBinding{};
    srcBinding.binding = 0;
    srcBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    srcBinding.descriptorCount = 1;
    srcBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding dstBinding{};
    dstBinding.binding = 1;
    dstBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    dstBinding.descriptorCount = 1;
    dstBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding normalBinding{};
    normalBinding.binding = 2;
    normalBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    normalBinding.descriptorCount = 1;
    normalBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    std::array<VkDescriptorSetLayoutBinding, 3> bindings = { srcBinding, dstBinding, normalBinding };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<u32>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(m_Context->GetDevice(), &layoutInfo, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create mipmap compute descriptor set layout");
    }
}

void VulkanMipmapCompute::CreatePipelineLayout() {
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(MipgenPushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_DescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(m_Context->GetDevice(), &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create mipmap compute pipeline layout");
    }
}

void VulkanMipmapCompute::CreateDescriptorPool() {
    constexpr u32 kMaxSets = 64;

    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    poolSizes[0].descriptorCount = kMaxSets * 2; // source + normal
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = kMaxSets;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = kMaxSets;

    if (vkCreateDescriptorPool(m_Context->GetDevice(), &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create mipmap compute descriptor pool");
    }
}

void VulkanMipmapCompute::CreatePipelines() {
    namespace fs = std::filesystem;

    const std::array<std::pair<Variant, const char*>, 4> shaderPaths = {{
        { Variant::Color, "assets/shaders/mipgen_color.comp.spv" },
        { Variant::Normal, "assets/shaders/mipgen_normal.comp.spv" },
        { Variant::Roughness, "assets/shaders/mipgen_roughness.comp.spv" },
        { Variant::Srgb, "assets/shaders/mipgen_srgb.comp.spv" }
    }};

    for (const auto& [variant, relativePath] : shaderPaths) {
        fs::path path = relativePath;

#ifdef ENGINE_SOURCE_DIR
        if (!fs::exists(path)) {
            path = fs::path(ENGINE_SOURCE_DIR) / path;
        }
#endif

        if (!fs::exists(path)) {
            throw std::runtime_error("Shader not found for mipmap compute: " + path.string());
        }

        std::vector<char> code = ReadBinaryFile(path);
        if (code.empty() || (code.size() % 4) != 0) {
            throw std::runtime_error("Invalid shader binary for mipmap compute: " + path.string());
        }

        VkShaderModuleCreateInfo moduleInfo{};
        moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleInfo.codeSize = code.size();
        moduleInfo.pCode = reinterpret_cast<const u32*>(code.data());

        VkShaderModule module = VK_NULL_HANDLE;
        if (vkCreateShaderModule(m_Context->GetDevice(), &moduleInfo, nullptr, &module) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module for mipmap compute: " + path.string());
        }

        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stageInfo.module = module;
        stageInfo.pName = "main";

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage = stageInfo;
        pipelineInfo.layout = m_PipelineLayout;

        VkPipeline pipeline = VK_NULL_HANDLE;
        VkResult result = vkCreateComputePipelines(m_Context->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

        vkDestroyShaderModule(m_Context->GetDevice(), module, nullptr);

        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute pipeline for mipmap generation");
        }

        m_Pipelines[static_cast<size_t>(variant)] = pipeline;
    }
}

VkPipeline VulkanMipmapCompute::GetPipeline(Variant variant) const {
    return m_Pipelines[static_cast<size_t>(variant)];
}

VkImageView VulkanMipmapCompute::CreateImageView(VkImage image,
                                                 VkFormat format,
                                                 u32 baseMipLevel,
                                                 u32 baseArrayLayer) const {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = baseMipLevel;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = baseArrayLayer;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView view = VK_NULL_HANDLE;
    if (vkCreateImageView(m_Context->GetDevice(), &viewInfo, nullptr, &view) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create sampled image view for mipmap compute");
    }
    return view;
}

VkImageView VulkanMipmapCompute::CreateStorageView(VkImage image,
                                                   VkFormat format,
                                                   u32 baseMipLevel,
                                                   u32 baseArrayLayer) const {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = baseMipLevel;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = baseArrayLayer;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView view = VK_NULL_HANDLE;
    if (vkCreateImageView(m_Context->GetDevice(), &viewInfo, nullptr, &view) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create storage image view for mipmap compute");
    }
    return view;
}

VkFormat VulkanMipmapCompute::GetStorageCompatibleFormat(VkFormat format) {
    switch (format) {
        // sRGB to linear conversions (storage images don't support sRGB)
        case VK_FORMAT_R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_UNORM;
        case VK_FORMAT_B8G8R8A8_SRGB: return VK_FORMAT_B8G8R8A8_UNORM;
        case VK_FORMAT_R8_SRGB: return VK_FORMAT_R8_UNORM;
        case VK_FORMAT_R8G8_SRGB: return VK_FORMAT_R8G8_UNORM;
        case VK_FORMAT_R8G8B8_SRGB: return VK_FORMAT_R8G8B8_UNORM;

        // BC compressed formats (sRGB variants)
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
        case VK_FORMAT_BC2_SRGB_BLOCK:
        case VK_FORMAT_BC3_SRGB_BLOCK:
        case VK_FORMAT_BC7_SRGB_BLOCK:
            // Compressed formats cannot be used as storage images
            // Caller must decompress first or use CPU path
            return VK_FORMAT_UNDEFINED;

        // Float/HDR formats - these typically support storage
        case VK_FORMAT_R16G16B16A16_SFLOAT:
        case VK_FORMAT_R32G32B32A32_SFLOAT:
        case VK_FORMAT_R16_SFLOAT:
        case VK_FORMAT_R16G16_SFLOAT:
        case VK_FORMAT_R32_SFLOAT:
        case VK_FORMAT_R32G32_SFLOAT:
        case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
        case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
            return format; // Float formats can be used as-is for storage

        // 10-bit formats
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
            return format; // 10-bit formats typically support storage

        // Standard unorm/snorm formats
        default:
            return format; // Assume format is already storage-compatible
    }
}

bool VulkanMipmapCompute::IsSrgbFormat(VkFormat format) {
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
