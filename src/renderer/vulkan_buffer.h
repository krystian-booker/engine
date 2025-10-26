#pragma once

#include "core/types.h"

#include <vulkan/vulkan.h>

class VulkanContext;

// Wraps a VkBuffer and its backing memory with convenient upload helpers.
class VulkanBuffer {
public:
    VulkanBuffer() = default;
    VulkanBuffer(VulkanBuffer&& other) noexcept;
    VulkanBuffer& operator=(VulkanBuffer&& other) noexcept;
    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;
    ~VulkanBuffer();

    void Create(
        VulkanContext* context,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE);

    void Destroy();

    void* Map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void Unmap();
    void Flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void Invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

    void CopyFrom(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

    VkBuffer GetBuffer() const { return m_Buffer; }
    VkDeviceMemory GetMemory() const { return m_Memory; }
    VkDeviceSize GetSize() const { return m_Size; }
    VkMemoryPropertyFlags GetMemoryProperties() const { return m_Properties; }
    VkBufferUsageFlags GetUsage() const { return m_Usage; }

    bool IsMapped() const { return m_MappedData != nullptr; }

private:
    VulkanContext* m_Context = nullptr;
    VkBuffer m_Buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_Memory = VK_NULL_HANDLE;
    VkDeviceSize m_Size = 0;
    VkDeviceSize m_AllocatedSize = 0;
    VkMemoryPropertyFlags m_Properties = 0;
    VkBufferUsageFlags m_Usage = 0;
    void* m_MappedData = nullptr;
    VkDeviceSize m_MapRange = VK_WHOLE_SIZE;
    VkDeviceSize m_MapOffset = 0;

    void Reset();
    u32 FindMemoryType(u32 typeFilter, VkMemoryPropertyFlags properties) const;
    void CopyFromHostVisible(const void* data, VkDeviceSize size, VkDeviceSize offset);
    void CopyFromDeviceLocal(const void* data, VkDeviceSize size, VkDeviceSize offset);
};
