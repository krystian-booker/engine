#include "renderer/vulkan_buffer.h"

#include "core/types.h"
#include "renderer/vulkan_context.h"

#include <cstring>
#include <stdexcept>

namespace {

VkDeviceSize ResolveRange(VkDeviceSize requested, VkDeviceSize fallback) {
    return requested == VK_WHOLE_SIZE ? fallback : requested;
}

} // namespace

VulkanBuffer::VulkanBuffer(VulkanBuffer&& other) noexcept {
    *this = std::move(other);
}

VulkanBuffer& VulkanBuffer::operator=(VulkanBuffer&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    Destroy();

    m_Context = other.m_Context;
    m_Buffer = other.m_Buffer;
    m_Memory = other.m_Memory;
    m_Size = other.m_Size;
    m_AllocatedSize = other.m_AllocatedSize;
    m_Properties = other.m_Properties;
    m_Usage = other.m_Usage;
    m_MappedData = other.m_MappedData;
    m_MapRange = other.m_MapRange;
    m_MapOffset = other.m_MapOffset;

    other.Reset();

    return *this;
}

VulkanBuffer::~VulkanBuffer() {
    Destroy();
}

void VulkanBuffer::Create(
    VulkanContext* context,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkSharingMode sharingMode) {

    if (context == nullptr) {
        throw std::invalid_argument("VulkanBuffer::Create requires valid context");
    }

    if (size == 0) {
        throw std::invalid_argument("VulkanBuffer::Create size must be greater than zero");
    }

    Destroy();

    m_Context = context;
    m_Size = size;
    m_Properties = properties;
    m_Usage = usage;

    VkDevice device = m_Context->GetDevice();

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = sharingMode;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &m_Buffer) != VK_SUCCESS) {
        Reset();
        throw std::runtime_error("Failed to create Vulkan buffer");
    }

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device, m_Buffer, &requirements);
    m_AllocatedSize = requirements.size;

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(requirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_Memory) != VK_SUCCESS) {
        vkDestroyBuffer(device, m_Buffer, nullptr);
        Reset();
        throw std::runtime_error("Failed to allocate Vulkan buffer memory");
    }

    if (vkBindBufferMemory(device, m_Buffer, m_Memory, 0) != VK_SUCCESS) {
        vkFreeMemory(device, m_Memory, nullptr);
        vkDestroyBuffer(device, m_Buffer, nullptr);
        Reset();
        throw std::runtime_error("Failed to bind Vulkan buffer memory");
    }
}

void VulkanBuffer::CreateAndUpload(
    VulkanContext* context,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    const void* data,
    VkSharingMode sharingMode) {

    if (data == nullptr && size != 0) {
        throw std::invalid_argument("VulkanBuffer::CreateAndUpload requires non-null data for non-zero size");
    }

    Create(context, size, usage, properties, sharingMode);

    if (size != 0) {
        CopyFrom(data, size, 0);
    }
}

void VulkanBuffer::Destroy() {
    if (m_Context == nullptr) {
        Reset();
        return;
    }

    VkDevice device = m_Context->GetDevice();
    if (device == VK_NULL_HANDLE) {
        Reset();
        return;
    }

    if (m_MappedData != nullptr) {
        vkUnmapMemory(device, m_Memory);
        m_MappedData = nullptr;
    }

    if (m_Buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_Buffer, nullptr);
    }

    if (m_Memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_Memory, nullptr);
    }

    Reset();
}

void* VulkanBuffer::Map(VkDeviceSize size, VkDeviceSize offset) {
    if (!(m_Properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
        throw std::runtime_error("VulkanBuffer::Map called on non-host-visible memory");
    }

    if (offset >= m_Size) {
        throw std::out_of_range("VulkanBuffer::Map offset out of range");
    }

    VkDeviceSize available = m_Size - offset;
    VkDeviceSize range = ResolveRange(size, available);
    if (range > available) {
        throw std::out_of_range("VulkanBuffer::Map size out of range");
    }

    if (m_MappedData == nullptr) {
        VkDevice device = m_Context->GetDevice();
        VkResult result = vkMapMemory(device, m_Memory, 0, VK_WHOLE_SIZE, 0, &m_MappedData);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to map Vulkan buffer memory");
        }
    }

    m_MapRange = range;
    m_MapOffset = offset;

    auto* bytes = static_cast<std::uint8_t*>(m_MappedData);
    return bytes + offset;
}

void VulkanBuffer::Unmap() {
    if (m_MappedData == nullptr) {
        return;
    }

    VkDevice device = m_Context->GetDevice();
    vkUnmapMemory(device, m_Memory);
    m_MappedData = nullptr;
    m_MapRange = VK_WHOLE_SIZE;
    m_MapOffset = 0;
}

void VulkanBuffer::Flush(VkDeviceSize size, VkDeviceSize offset) {
    if (!(m_Properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
        return;
    }

    if (m_Context == nullptr || m_Memory == VK_NULL_HANDLE) {
        return;
    }

    if (m_Properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
        return;
    }

    VkDeviceSize range = ResolveRange(size, m_MapRange == VK_WHOLE_SIZE ? m_Size : m_MapRange);
    VkDeviceSize flushOffset = offset == VK_WHOLE_SIZE ? m_MapOffset : offset;

    VkMappedMemoryRange flushRange{};
    flushRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    flushRange.memory = m_Memory;
    flushRange.offset = flushOffset;
    flushRange.size = range;

    VkDevice device = m_Context->GetDevice();
    if (vkFlushMappedMemoryRanges(device, 1, &flushRange) != VK_SUCCESS) {
        throw std::runtime_error("Failed to flush Vulkan buffer memory");
    }
}

void VulkanBuffer::Invalidate(VkDeviceSize size, VkDeviceSize offset) {
    if (!(m_Properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
        return;
    }

    if (m_Context == nullptr || m_Memory == VK_NULL_HANDLE) {
        return;
    }

    if (m_Properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
        return;
    }

    VkDeviceSize range = ResolveRange(size, m_MapRange == VK_WHOLE_SIZE ? m_Size : m_MapRange);
    VkDeviceSize invalidateOffset = offset == VK_WHOLE_SIZE ? m_MapOffset : offset;

    VkMappedMemoryRange invalidateRange{};
    invalidateRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    invalidateRange.memory = m_Memory;
    invalidateRange.offset = invalidateOffset;
    invalidateRange.size = range;

    VkDevice device = m_Context->GetDevice();
    if (vkInvalidateMappedMemoryRanges(device, 1, &invalidateRange) != VK_SUCCESS) {
        throw std::runtime_error("Failed to invalidate Vulkan buffer memory");
    }
}

void VulkanBuffer::CopyFrom(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    if (data == nullptr) {
        throw std::invalid_argument("VulkanBuffer::CopyFrom requires valid source data");
    }

    if (size == 0) {
        return;
    }

    if (offset + size > m_Size) {
        throw std::out_of_range("VulkanBuffer::CopyFrom exceeds buffer bounds");
    }

    if (m_Properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        CopyFromHostVisible(data, size, offset);
    } else {
        CopyFromDeviceLocal(data, size, offset);
    }
}

void VulkanBuffer::Reset() {
    m_Context = nullptr;
    m_Buffer = VK_NULL_HANDLE;
    m_Memory = VK_NULL_HANDLE;
    m_Size = 0;
    m_AllocatedSize = 0;
    m_Properties = 0;
    m_Usage = 0;
    m_MappedData = nullptr;
    m_MapRange = VK_WHOLE_SIZE;
    m_MapOffset = 0;
}

u32 VulkanBuffer::FindMemoryType(u32 typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(m_Context->GetPhysicalDevice(), &memoryProperties);

    for (u32 i = 0; i < memoryProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) != 0u &&
            (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable Vulkan memory type");
}

void VulkanBuffer::CopyFromHostVisible(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    bool wasMapped = m_MappedData != nullptr;
    auto* dst = static_cast<std::uint8_t*>(Map(size, offset));
    std::memcpy(dst, data, static_cast<size_t>(size));

    if ((m_Properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
        Flush(size, offset);
    }

    if (!wasMapped) {
        Unmap();
    }
}

void VulkanBuffer::CopyFromDeviceLocal(const void* data, VkDeviceSize size, VkDeviceSize offset) {
    VulkanBuffer staging;
    staging.Create(
        m_Context,
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    staging.CopyFromHostVisible(data, size, 0);

    VkDevice device = m_Context->GetDevice();
    VkCommandPool commandPool = m_Context->GetCommandPool();
    if (commandPool == VK_NULL_HANDLE) {
        staging.Destroy();
        throw std::runtime_error("Vulkan context does not provide a command pool for buffer copy");
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        staging.Destroy();
        throw std::runtime_error("Failed to allocate command buffer for buffer copy");
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        staging.Destroy();
        throw std::runtime_error("Failed to begin command buffer for buffer copy");
    }

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = offset;
    copyRegion.size = size;

    vkCmdCopyBuffer(commandBuffer, staging.GetBuffer(), m_Buffer, 1, &copyRegion);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        staging.Destroy();
        throw std::runtime_error("Failed to record buffer copy command");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkQueue queue = m_Context->GetGraphicsQueue();
    if (vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        staging.Destroy();
        throw std::runtime_error("Failed to submit buffer copy command");
    }

    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);

    staging.Destroy();
}
