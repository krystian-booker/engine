#include "renderer/vulkan_command_buffer.h"

#include "renderer/vulkan_context.h"

#include <stdexcept>

VulkanCommandBuffer::~VulkanCommandBuffer() {
    Shutdown();
}

void VulkanCommandBuffer::Init(VulkanContext* context, u32 count) {
    if (!context) {
        throw std::invalid_argument("VulkanCommandBuffer::Init requires valid context");
    }

    if (count == 0) {
        throw std::invalid_argument("VulkanCommandBuffer::Init requires at least one command buffer");
    }

    Shutdown();

    m_Context = context;

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_Context->GetGraphicsQueueFamily();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkDevice device = m_Context->GetDevice();
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &m_CommandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }

    m_CommandBuffers.resize(count, VK_NULL_HANDLE);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_CommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = count;

    if (vkAllocateCommandBuffers(device, &allocInfo, m_CommandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers");
    }
}

void VulkanCommandBuffer::Shutdown() {
    if (m_Context && m_CommandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_Context->GetDevice(), m_CommandPool, nullptr);
    }

    m_CommandBuffers.clear();
    m_CommandPool = VK_NULL_HANDLE;
    m_Context = nullptr;
}

void VulkanCommandBuffer::Reset(u32 index) {
    if (index >= m_CommandBuffers.size()) {
        throw std::out_of_range("VulkanCommandBuffer::Reset index out of range");
    }

    if (vkResetCommandBuffer(m_CommandBuffers[index], 0) != VK_SUCCESS) {
        throw std::runtime_error("Failed to reset command buffer");
    }
}

VkCommandBuffer VulkanCommandBuffer::Get(u32 index) const {
    if (index >= m_CommandBuffers.size()) {
        throw std::out_of_range("VulkanCommandBuffer::Get index out of range");
    }

    return m_CommandBuffers[index];
}
