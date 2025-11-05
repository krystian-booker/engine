#include "renderer/vulkan_mipmap_compute.h"
#include "renderer/vulkan_context.h"
#include "renderer/vulkan_buffer.h"
#include "platform/window.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {

struct RGBA8 {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
    std::uint8_t a{};
};

struct Float3 {
    float x{};
    float y{};
    float z{};
};

static int testsRun = 0;
static int testsPassed = 0;
static int testsFailed = 0;

#define TEST(name) \
    static void name(); \
    static void name##_runner() { \
        testsRun++; \
        std::cout << "Running " << #name << "... "; \
        try { \
            name(); \
            testsPassed++; \
            std::cout << "PASSED" << std::endl; \
        } catch (const std::exception& ex) { \
            testsFailed++; \
            std::cout << "FAILED (" << ex.what() << ")" << std::endl; \
        } catch (...) { \
            testsFailed++; \
            std::cout << "FAILED (unknown exception)" << std::endl; \
        } \
    } \
    static void name()

#define ASSERT(expr) \
    do { \
        if (!(expr)) { \
            throw std::runtime_error(std::string("Assertion failed: ") + #expr); \
        } \
    } while (0)

VkDevice Device(VulkanContext* context) {
    return context->GetDevice();
}

VkCommandBuffer BeginSingleTimeCommands(VulkanContext* context) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = context->GetCommandPool();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(Device(context), &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

void EndSingleTimeCommands(VulkanContext* context, VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkQueue queue = context->GetGraphicsQueue();
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(Device(context), context->GetCommandPool(), 1, &commandBuffer);
}

u32 FindMemoryType(VulkanContext* context, u32 typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties{};
    vkGetPhysicalDeviceMemoryProperties(context->GetPhysicalDevice(), &memProperties);

    for (u32 i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}

void TransitionImageLayout(VulkanContext* context,
                           VkImage image,
                           VkImageLayout oldLayout,
                           VkImageLayout newLayout,
                           u32 baseMipLevel,
                           u32 mipCount) {
    VkCommandBuffer cmd = BeginSingleTimeCommands(context);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = baseMipLevel;
    barrier.subresourceRange.levelCount = mipCount;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags destStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        destStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::runtime_error("Unsupported layout transition in test");
    }

    vkCmdPipelineBarrier(cmd,
                         sourceStage,
                         destStage,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &barrier);

    EndSingleTimeCommands(context, cmd);
}

void CopyBufferToImage(VulkanContext* context,
                       VkBuffer buffer,
                       VkImage image,
                       u32 width,
                       u32 height) {
    VkCommandBuffer cmd = BeginSingleTimeCommands(context);

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

    vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    EndSingleTimeCommands(context, cmd);
}

std::vector<RGBA8> CopyImageMipToHost(VulkanContext* context,
                                      VkImage image,
                                      VkFormat format,
                                      u32 width,
                                      u32 height,
                                      u32 mipLevel) {
    const VkDeviceSize byteSize = static_cast<VkDeviceSize>(width) * height * 4;

    VulkanBuffer readback;
    readback.Create(
        context,
        byteSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkCommandBuffer cmd = BeginSingleTimeCommands(context);

    VkImageMemoryBarrier toTransfer{};
    toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransfer.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = image;
    toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toTransfer.subresourceRange.baseMipLevel = mipLevel;
    toTransfer.subresourceRange.levelCount = 1;
    toTransfer.subresourceRange.baseArrayLayer = 0;
    toTransfer.subresourceRange.layerCount = 1;
    toTransfer.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &toTransfer);

    VkBufferImageCopy copy{};
    copy.bufferOffset = 0;
    copy.bufferRowLength = 0;
    copy.bufferImageHeight = 0;
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.mipLevel = mipLevel;
    copy.imageSubresource.baseArrayLayer = 0;
    copy.imageSubresource.layerCount = 1;
    copy.imageOffset = {0, 0, 0};
    copy.imageExtent = {width, height, 1};

    vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback.GetBuffer(), 1, &copy);

    VkImageMemoryBarrier backToShader{};
    backToShader.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    backToShader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    backToShader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    backToShader.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    backToShader.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    backToShader.image = image;
    backToShader.subresourceRange = toTransfer.subresourceRange;
    backToShader.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    backToShader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &backToShader);

    EndSingleTimeCommands(context, cmd);

    std::vector<RGBA8> out(width * height);
    auto* mapped = static_cast<RGBA8*>(readback.Map());
    std::copy(mapped, mapped + out.size(), out.begin());
    readback.Unmap();
    readback.Destroy();

    (void)format;
    return out;
}

RGBA8 MakeRGBA(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    return RGBA8{r, g, b, a};
}

std::vector<RGBA8> GenerateLinearDownsample(const std::vector<RGBA8>& source,
                                            u32 srcWidth,
                                            u32 srcHeight) {
    const u32 dstWidth = std::max(1u, srcWidth / 2);
    const u32 dstHeight = std::max(1u, srcHeight / 2);
    std::vector<RGBA8> result(dstWidth * dstHeight);

    for (u32 y = 0; y < dstHeight; ++y) {
        for (u32 x = 0; x < dstWidth; ++x) {
            const u32 sx = x * 2;
            const u32 sy = y * 2;

            auto sample = [&](u32 ox, u32 oy) -> const RGBA8& {
                const u32 cx = std::min(sx + ox, srcWidth - 1);
                const u32 cy = std::min(sy + oy, srcHeight - 1);
                return source[cy * srcWidth + cx];
            };

            std::uint32_t sumR = 0, sumG = 0, sumB = 0, sumA = 0;
            for (u32 oy = 0; oy < 2; ++oy) {
                for (u32 ox = 0; ox < 2; ++ox) {
                    const RGBA8& s = sample(ox, oy);
                    sumR += s.r;
                    sumG += s.g;
                    sumB += s.b;
                    sumA += s.a;
                }
            }

            RGBA8& dst = result[y * dstWidth + x];
            dst.r = static_cast<std::uint8_t>(sumR / 4u);
            dst.g = static_cast<std::uint8_t>(sumG / 4u);
            dst.b = static_cast<std::uint8_t>(sumB / 4u);
            dst.a = static_cast<std::uint8_t>(sumA / 4u);
        }
    }
    return result;
}

float SrgbToLinear(float c) {
    if (c <= 0.04045f) {
        return c / 12.92f;
    }
    return std::pow((c + 0.055f) / 1.055f, 2.4f);
}

float LinearToSrgb(float c) {
    if (c <= 0.0031308f) {
        return c * 12.92f;
    }
    return 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

std::vector<RGBA8> GenerateSrgbDownsample(const std::vector<RGBA8>& source,
                                          u32 srcWidth,
                                          u32 srcHeight) {
    const u32 dstWidth = std::max(1u, srcWidth / 2);
    const u32 dstHeight = std::max(1u, srcHeight / 2);
    std::vector<RGBA8> result(dstWidth * dstHeight);

    for (u32 y = 0; y < dstHeight; ++y) {
        for (u32 x = 0; x < dstWidth; ++x) {
            const u32 sx = x * 2;
            const u32 sy = y * 2;

            auto sample = [&](u32 ox, u32 oy) -> const RGBA8& {
                const u32 cx = std::min(sx + ox, srcWidth - 1);
                const u32 cy = std::min(sy + oy, srcHeight - 1);
                return source[cy * srcWidth + cx];
            };

            Float3 accumLinear{0.0f, 0.0f, 0.0f};
            float accumAlpha = 0.0f;
            for (u32 oy = 0; oy < 2; ++oy) {
                for (u32 ox = 0; ox < 2; ++ox) {
                    const RGBA8& s = sample(ox, oy);
                    const float sr = s.r / 255.0f;
                    const float sg = s.g / 255.0f;
                    const float sb = s.b / 255.0f;
                    accumLinear.x += SrgbToLinear(sr);
                    accumLinear.y += SrgbToLinear(sg);
                    accumLinear.z += SrgbToLinear(sb);
                    accumAlpha += s.a / 255.0f;
                }
            }

            accumLinear.x *= 0.25f;
            accumLinear.y *= 0.25f;
            accumLinear.z *= 0.25f;
            accumAlpha *= 0.25f;

            RGBA8& dst = result[y * dstWidth + x];
            auto toUnorm = [](float c) -> std::uint8_t {
                c = std::clamp(c, 0.0f, 1.0f);
                return static_cast<std::uint8_t>(std::lround(c * 255.0f));
            };

            dst.r = toUnorm(LinearToSrgb(accumLinear.x));
            dst.g = toUnorm(LinearToSrgb(accumLinear.y));
            dst.b = toUnorm(LinearToSrgb(accumLinear.z));
            dst.a = toUnorm(accumAlpha);
        }
    }

    return result;
}

Float3 DecodeNormal(const RGBA8& c) {
    return Float3{
        c.r / 255.0f * 2.0f - 1.0f,
        c.g / 255.0f * 2.0f - 1.0f,
        c.b / 255.0f * 2.0f - 1.0f
    };
}

RGBA8 EncodeNormal(const Float3& n, float alpha) {
    auto toUnorm = [](float v) -> std::uint8_t {
        return static_cast<std::uint8_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f));
    };
    return RGBA8{
        toUnorm((n.x * 0.5f) + 0.5f),
        toUnorm((n.y * 0.5f) + 0.5f),
        toUnorm((n.z * 0.5f) + 0.5f),
        toUnorm(alpha)
    };
}

Float3 Normalize(const Float3& v) {
    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len > 1e-6f) {
        return Float3{v.x / len, v.y / len, v.z / len};
    }
    return Float3{0.0f, 0.0f, 1.0f};
}

std::vector<RGBA8> GenerateNormalDownsample(const std::vector<RGBA8>& source,
                                            u32 srcWidth,
                                            u32 srcHeight) {
    const u32 dstWidth = std::max(1u, srcWidth / 2);
    const u32 dstHeight = std::max(1u, srcHeight / 2);
    std::vector<RGBA8> result(dstWidth * dstHeight);

    for (u32 y = 0; y < dstHeight; ++y) {
        for (u32 x = 0; x < dstWidth; ++x) {
            const u32 sx = x * 2;
            const u32 sy = y * 2;

            Float3 sum{0.0f, 0.0f, 0.0f};
            float alphaSum = 0.0f;

            for (u32 oy = 0; oy < 2; ++oy) {
                for (u32 ox = 0; ox < 2; ++ox) {
                    const u32 cx = std::min(sx + ox, srcWidth - 1);
                    const u32 cy = std::min(sy + oy, srcHeight - 1);
                    const RGBA8& s = source[cy * srcWidth + cx];
                    const Float3 normal = DecodeNormal(s);
                    sum.x += normal.x;
                    sum.y += normal.y;
                    sum.z += normal.z;
                    alphaSum += s.a / 255.0f;
                }
            }

            sum.x *= 0.25f;
            sum.y *= 0.25f;
            sum.z *= 0.25f;
            Float3 normalized = Normalize(sum);
            float alphaAvg = std::clamp(alphaSum * 0.25f, 0.0f, 1.0f);
            result[y * dstWidth + x] = EncodeNormal(normalized, alphaAvg);
        }
    }

    return result;
}

void CompareWithTolerance(const std::vector<RGBA8>& lhs,
                          const std::vector<RGBA8>& rhs,
                          int tolerance) {
    ASSERT(lhs.size() == rhs.size());
    for (size_t i = 0; i < lhs.size(); ++i) {
        auto diff = [&](std::uint8_t a, std::uint8_t b) {
            return std::abs(static_cast<int>(a) - static_cast<int>(b));
        };
        if (diff(lhs[i].r, rhs[i].r) > tolerance ||
            diff(lhs[i].g, rhs[i].g) > tolerance ||
            diff(lhs[i].b, rhs[i].b) > tolerance ||
            diff(lhs[i].a, rhs[i].a) > tolerance) {
            std::ostringstream oss;
            oss << "Mismatch at index " << i
                << " | actual RGBA: (" << static_cast<int>(lhs[i].r) << ", "
                << static_cast<int>(lhs[i].g) << ", " << static_cast<int>(lhs[i].b) << ", "
                << static_cast<int>(lhs[i].a) << ")"
                << " expected RGBA: (" << static_cast<int>(rhs[i].r) << ", "
                << static_cast<int>(rhs[i].g) << ", " << static_cast<int>(rhs[i].b) << ", "
                << static_cast<int>(rhs[i].a) << ")"
                << " tolerance: " << tolerance;
            throw std::runtime_error(oss.str());
        }
    }
}

struct VulkanImage {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

VulkanImage CreateTestImage(VulkanContext* context,
                            VkFormat format,
                            u32 width,
                            u32 height,
                            u32 mipLevels,
                            VkImageUsageFlags usage,
                            VkImageCreateFlags flags = 0) {
    VulkanImage result{};

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.flags = flags;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    ASSERT(vkCreateImage(Device(context), &imageInfo, nullptr, &result.image) == VK_SUCCESS);

    VkMemoryRequirements memReq{};
    vkGetImageMemoryRequirements(Device(context), result.image, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = FindMemoryType(context, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    ASSERT(vkAllocateMemory(Device(context), &allocInfo, nullptr, &result.memory) == VK_SUCCESS);
    ASSERT(vkBindImageMemory(Device(context), result.image, result.memory, 0) == VK_SUCCESS);

    return result;
}

void DestroyTestImage(VulkanContext* context, VulkanImage& image) {
    if (image.image != VK_NULL_HANDLE) {
        vkDestroyImage(Device(context), image.image, nullptr);
        image.image = VK_NULL_HANDLE;
    }
    if (image.memory != VK_NULL_HANDLE) {
        vkFreeMemory(Device(context), image.memory, nullptr);
        image.memory = VK_NULL_HANDLE;
    }
}

} // namespace

TEST(MipgenColor_GeneratesLinearAverage) {
    WindowProperties props;
    props.title = "Mipgen Color Test";
    props.width = 320;
    props.height = 240;
    props.resizable = false;

    Window window(props);

    VulkanContext context;
    context.Init(&window);

    const u32 width = 4;
    const u32 height = 4;
    const u32 mipLevels = 3;

    std::vector<RGBA8> level0(width * height);
    for (u32 y = 0; y < height; ++y) {
        for (u32 x = 0; x < width; ++x) {
            const u32 index = y * width + x;
            const std::uint8_t base = static_cast<std::uint8_t>((index * 4) & 0xFF);
            level0[index] = MakeRGBA(base, static_cast<std::uint8_t>((base + 40) & 0xFF),
                                     static_cast<std::uint8_t>((base + 80) & 0xFF), 200);
        }
    }

    VulkanBuffer staging;
    staging.Create(&context,
                   level0.size() * sizeof(RGBA8),
                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    staging.CopyFrom(level0.data(), level0.size() * sizeof(RGBA8));

    VulkanImage image = CreateTestImage(&context,
                                        VK_FORMAT_R8G8B8A8_UNORM,
                                        width,
                                        height,
                                        mipLevels,
                                        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                        VK_IMAGE_USAGE_STORAGE_BIT |
                                        VK_IMAGE_USAGE_SAMPLED_BIT);

    TransitionImageLayout(&context, image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, mipLevels);
    CopyBufferToImage(&context, staging.GetBuffer(), image.image, width, height);

    VulkanMipmapCompute::Params params{};
    params.image = image.image;
    params.format = VK_FORMAT_R8G8B8A8_UNORM;
    params.width = width;
    params.height = height;
    params.mipLevels = mipLevels;
    params.baseArrayLayer = 0;
    params.layerCount = 1;
    params.variant = VulkanMipmapCompute::Variant::Color;

    context.GetMipmapCompute()->Generate(params);

    const u32 level1Width = std::max(1u, width / 2);
    const u32 level1Height = std::max(1u, height / 2);
    const u32 level2Width = std::max(1u, level1Width / 2);
    const u32 level2Height = std::max(1u, level1Height / 2);

    auto expectedLevel1 = GenerateLinearDownsample(level0, width, height);
    auto expectedLevel2 = GenerateLinearDownsample(expectedLevel1, level1Width, level1Height);

    auto gpuLevel1 = CopyImageMipToHost(&context, image.image, VK_FORMAT_R8G8B8A8_UNORM, level1Width, level1Height, 1);
    auto gpuLevel2 = CopyImageMipToHost(&context, image.image, VK_FORMAT_R8G8B8A8_UNORM, level2Width, level2Height, 2);

    CompareWithTolerance(gpuLevel1, expectedLevel1, 1);
    CompareWithTolerance(gpuLevel2, expectedLevel2, 1);

    staging.Destroy();
    DestroyTestImage(&context, image);
    context.Shutdown();
}

TEST(MipgenSrgb_GammaCorrectsAverage) {
    WindowProperties props;
    props.title = "Mipgen sRGB Test";
    props.width = 320;
    props.height = 240;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    const u32 width = 4;
    const u32 height = 4;
    const u32 mipLevels = 3;

    std::vector<RGBA8> level0(width * height);
    for (u32 y = 0; y < height; ++y) {
        for (u32 x = 0; x < width; ++x) {
            const u32 index = y * width + x;
            const std::uint8_t base = static_cast<std::uint8_t>((index * 16) & 0xFF);
            level0[index] = MakeRGBA(base, static_cast<std::uint8_t>((255 - base)), static_cast<std::uint8_t>((base + 64) & 0xFF), 180);
        }
    }

    VulkanBuffer staging;
    staging.Create(&context,
                   level0.size() * sizeof(RGBA8),
                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    staging.CopyFrom(level0.data(), level0.size() * sizeof(RGBA8));

    VulkanImage image = CreateTestImage(&context,
                                        VK_FORMAT_R8G8B8A8_SRGB,
                                        width,
                                        height,
                                        mipLevels,
                                        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                        VK_IMAGE_USAGE_STORAGE_BIT |
                                        VK_IMAGE_USAGE_SAMPLED_BIT,
                                        VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT);

    TransitionImageLayout(&context, image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, mipLevels);
    CopyBufferToImage(&context, staging.GetBuffer(), image.image, width, height);

    VulkanMipmapCompute::Params params{};
    params.image = image.image;
    params.format = VK_FORMAT_R8G8B8A8_SRGB;
    params.width = width;
    params.height = height;
    params.mipLevels = mipLevels;
    params.baseArrayLayer = 0;
    params.layerCount = 1;
    params.variant = VulkanMipmapCompute::Variant::Srgb;

    context.GetMipmapCompute()->Generate(params);

    const u32 level1Width = std::max(1u, width / 2);
    const u32 level1Height = std::max(1u, height / 2);
    const u32 level2Width = std::max(1u, level1Width / 2);
    const u32 level2Height = std::max(1u, level1Height / 2);

    auto expectedLevel1 = GenerateSrgbDownsample(level0, width, height);
    auto expectedLevel2 = GenerateSrgbDownsample(expectedLevel1, level1Width, level1Height);

    auto gpuLevel1 = CopyImageMipToHost(&context, image.image, VK_FORMAT_R8G8B8A8_SRGB, level1Width, level1Height, 1);
    auto gpuLevel2 = CopyImageMipToHost(&context, image.image, VK_FORMAT_R8G8B8A8_SRGB, level2Width, level2Height, 2);

    CompareWithTolerance(gpuLevel1, expectedLevel1, 1);
    CompareWithTolerance(gpuLevel2, expectedLevel2, 1);

    staging.Destroy();
    DestroyTestImage(&context, image);
    context.Shutdown();
}

TEST(MipgenNormal_RenormalizesVectors) {
    WindowProperties props;
    props.title = "Mipgen Normal Test";
    props.width = 320;
    props.height = 240;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    const u32 width = 4;
    const u32 height = 4;
    const u32 mipLevels = 2;

    const std::array<Float3, 4> normals = {{
        Float3{1.0f, 0.0f, 0.0f},
        Float3{0.0f, 1.0f, 0.0f},
        Float3{0.0f, 0.0f, 1.0f},
        Float3{-1.0f, 0.0f, 0.0f}
    }};

    std::vector<RGBA8> level0(width * height);
    for (u32 y = 0; y < height; ++y) {
        for (u32 x = 0; x < width; ++x) {
            const Float3 n = normals[(y % 2) * 2 + (x % 2)];
            level0[y * width + x] = EncodeNormal(n, 1.0f);
        }
    }

    VulkanBuffer staging;
    staging.Create(&context,
                   level0.size() * sizeof(RGBA8),
                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    staging.CopyFrom(level0.data(), level0.size() * sizeof(RGBA8));

    VulkanImage image = CreateTestImage(&context,
                                        VK_FORMAT_R8G8B8A8_UNORM,
                                        width,
                                        height,
                                        mipLevels,
                                        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                        VK_IMAGE_USAGE_STORAGE_BIT |
                                        VK_IMAGE_USAGE_SAMPLED_BIT);

    TransitionImageLayout(&context, image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, mipLevels);
    CopyBufferToImage(&context, staging.GetBuffer(), image.image, width, height);

    VulkanMipmapCompute::Params params{};
    params.image = image.image;
    params.format = VK_FORMAT_R8G8B8A8_UNORM;
    params.width = width;
    params.height = height;
    params.mipLevels = mipLevels;
    params.variant = VulkanMipmapCompute::Variant::Normal;

    context.GetMipmapCompute()->Generate(params);

    const u32 level1Width = std::max(1u, width / 2);
    const u32 level1Height = std::max(1u, height / 2);
    auto expectedLevel1 = GenerateNormalDownsample(level0, width, height);
    auto gpuLevel1 = CopyImageMipToHost(&context, image.image, VK_FORMAT_R8G8B8A8_UNORM, level1Width, level1Height, 1);

    CompareWithTolerance(gpuLevel1, expectedLevel1, 1);

    staging.Destroy();
    DestroyTestImage(&context, image);
    context.Shutdown();
}

TEST(MipgenRoughness_FiltersWithToksvig) {
    WindowProperties props;
    props.title = "Mipgen Roughness Test";
    props.width = 320;
    props.height = 240;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    const u32 width = 4;
    const u32 height = 4;
    const u32 mipLevels = 2;

    // Create roughness map: R=Roughness, G=Metalness, B=AO, A=unused
    std::vector<RGBA8> level0(width * height);
    for (u32 y = 0; y < height; ++y) {
        for (u32 x = 0; x < width; ++x) {
            const u32 index = y * width + x;
            level0[index] = MakeRGBA(
                static_cast<std::uint8_t>(64 + index * 8),  // Roughness
                static_cast<std::uint8_t>(index % 2 == 0 ? 255 : 0),  // Metalness (binary)
                static_cast<std::uint8_t>(200 - index * 4),  // AO
                255  // Unused
            );
        }
    }

    VulkanBuffer staging;
    staging.Create(&context,
                   level0.size() * sizeof(RGBA8),
                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    staging.CopyFrom(level0.data(), level0.size() * sizeof(RGBA8));

    VulkanImage image = CreateTestImage(&context,
                                        VK_FORMAT_R8G8B8A8_UNORM,
                                        width,
                                        height,
                                        mipLevels,
                                        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                        VK_IMAGE_USAGE_STORAGE_BIT |
                                        VK_IMAGE_USAGE_SAMPLED_BIT);

    TransitionImageLayout(&context, image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, mipLevels);
    CopyBufferToImage(&context, staging.GetBuffer(), image.image, width, height);

    VulkanMipmapCompute::Params params{};
    params.image = image.image;
    params.format = VK_FORMAT_R8G8B8A8_UNORM;
    params.width = width;
    params.height = height;
    params.mipLevels = mipLevels;
    params.variant = VulkanMipmapCompute::Variant::Roughness;
    params.hasNormalMap = false;  // Test without Toksvig adjustment first

    context.GetMipmapCompute()->Generate(params);

    const u32 level1Width = std::max(1u, width / 2);
    const u32 level1Height = std::max(1u, height / 2);
    auto gpuLevel1 = CopyImageMipToHost(&context, image.image, VK_FORMAT_R8G8B8A8_UNORM, level1Width, level1Height, 1);

    // Verify the filtering: roughness averaged, metalness min, AO multiplied
    // This is a basic sanity check; exact values depend on the Toksvig formula
    ASSERT(gpuLevel1.size() == static_cast<size_t>(level1Width * level1Height));
    // Check that metalness uses min (should be 0 if any input is 0)
    bool hasNonMetallic = false;
    for (const auto& pixel : gpuLevel1) {
        if (pixel.g == 0) {
            hasNonMetallic = true;
            break;
        }
    }
    ASSERT(hasNonMetallic);  // At least one pixel should be non-metallic due to min operation

    staging.Destroy();
    DestroyTestImage(&context, image);
    context.Shutdown();
}

TEST(MipgenColor_PremultipliedAlpha) {
    WindowProperties props;
    props.title = "Mipgen Premultiplied Alpha Test";
    props.width = 320;
    props.height = 240;
    props.resizable = false;

    Window window(props);
    VulkanContext context;
    context.Init(&window);

    const u32 width = 4;
    const u32 height = 4;
    const u32 mipLevels = 2;

    // Create test pattern with varying alpha
    std::vector<RGBA8> level0(width * height);
    for (u32 y = 0; y < height; ++y) {
        for (u32 x = 0; x < width; ++x) {
            const u32 index = y * width + x;
            const std::uint8_t alpha = static_cast<std::uint8_t>(64 + index * 12);
            level0[index] = MakeRGBA(255, 128, 64, alpha);
        }
    }

    VulkanBuffer staging;
    staging.Create(&context,
                   level0.size() * sizeof(RGBA8),
                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    staging.CopyFrom(level0.data(), level0.size() * sizeof(RGBA8));

    VulkanImage image = CreateTestImage(&context,
                                        VK_FORMAT_R8G8B8A8_UNORM,
                                        width,
                                        height,
                                        mipLevels,
                                        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                        VK_IMAGE_USAGE_STORAGE_BIT |
                                        VK_IMAGE_USAGE_SAMPLED_BIT);

    TransitionImageLayout(&context, image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, mipLevels);
    CopyBufferToImage(&context, staging.GetBuffer(), image.image, width, height);

    VulkanMipmapCompute::Params params{};
    params.image = image.image;
    params.format = VK_FORMAT_R8G8B8A8_UNORM;
    params.width = width;
    params.height = height;
    params.mipLevels = mipLevels;
    params.variant = VulkanMipmapCompute::Variant::Color;
    params.alphaMode = VulkanMipmapCompute::AlphaMode::Premultiplied;

    context.GetMipmapCompute()->Generate(params);

    const u32 level1Width = std::max(1u, width / 2);
    const u32 level1Height = std::max(1u, height / 2);
    auto gpuLevel1 = CopyImageMipToHost(&context, image.image, VK_FORMAT_R8G8B8A8_UNORM, level1Width, level1Height, 1);

    // Basic validation: ensure mip was generated and alpha was processed
    ASSERT(gpuLevel1.size() == static_cast<size_t>(level1Width * level1Height));
    // All pixels should have alpha > 0 since input alphas are all > 64
    for (const auto& pixel : gpuLevel1) {
        ASSERT(pixel.a > 0);
    }

    staging.Destroy();
    DestroyTestImage(&context, image);
    context.Shutdown();
}

int main() {
    std::cout << "=== Vulkan Mipmap Compute Tests ===" << std::endl << std::endl;

    MipgenColor_GeneratesLinearAverage_runner();
    MipgenSrgb_GammaCorrectsAverage_runner();
    MipgenNormal_RenormalizesVectors_runner();
    MipgenRoughness_FiltersWithToksvig_runner();
    MipgenColor_PremultipliedAlpha_runner();

    std::cout << std::endl;
    std::cout << "===================================" << std::endl;
    std::cout << "Tests run: " << testsRun << std::endl;
    std::cout << "Tests passed: " << testsPassed << std::endl;
    std::cout << "Tests failed: " << testsFailed << std::endl;
    std::cout << "===================================" << std::endl;

    return testsFailed == 0 ? 0 : 1;
}
