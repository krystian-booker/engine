#pragma once

#include "core/types.h"

#include <vulkan/vulkan.h>

#include <vector>

class VulkanContext;

// Manages the primary command pool and per-frame command buffers.
class VulkanCommandBuffer {
public:
    VulkanCommandBuffer() = default;
    ~VulkanCommandBuffer();

    void Init(VulkanContext* context, u32 count);
    void Shutdown();

    void Reset(u32 index);

    VkCommandPool GetCommandPool() const { return m_CommandPool; }
    const std::vector<VkCommandBuffer>& GetCommandBuffers() const { return m_CommandBuffers; }
    VkCommandBuffer Get(u32 index) const;

private:
    VulkanContext* m_Context = nullptr;
    VkCommandPool m_CommandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_CommandBuffers;
};
