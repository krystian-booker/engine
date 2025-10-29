#include "vulkan_material_buffer.h"
#include "vulkan_context.h"
#include <stdexcept>
#include <cstring>
#include <iostream>

VulkanMaterialBuffer::~VulkanMaterialBuffer() {
    Shutdown();
}

void VulkanMaterialBuffer::Init(VulkanContext* context, u32 initialCapacity) {
    if (!context) {
        throw std::invalid_argument("VulkanMaterialBuffer::Init requires valid context");
    }

    if (initialCapacity == 0) {
        throw std::invalid_argument("VulkanMaterialBuffer::Init requires non-zero capacity");
    }

    Shutdown();

    m_Context = context;
    m_Capacity = initialCapacity;
    m_MaterialCount = 0;

    // Create storage buffer (device-local with staging support)
    VkDeviceSize bufferSize = m_Capacity * sizeof(GPUMaterial);

    m_Buffer.Create(
        m_Context,
        bufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    std::cout << "VulkanMaterialBuffer initialized with capacity: " << m_Capacity << " materials" << std::endl;
}

void VulkanMaterialBuffer::Shutdown() {
    if (m_Buffer.GetBuffer() != VK_NULL_HANDLE) {
        m_Buffer.Destroy();
    }

    m_Context = nullptr;
    m_Capacity = 0;
    m_MaterialCount = 0;
}

u32 VulkanMaterialBuffer::UploadMaterial(const GPUMaterial& material) {
    if (!m_Context) {
        throw std::runtime_error("VulkanMaterialBuffer::UploadMaterial called before Init()");
    }

    // Check if we need to resize
    if (m_MaterialCount >= m_Capacity) {
        Resize(m_Capacity * 2);  // Double capacity
    }

    u32 index = m_MaterialCount++;

    // Upload material data using staging buffer
    VulkanBuffer stagingBuffer;
    stagingBuffer.Create(
        m_Context,
        sizeof(GPUMaterial),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    // Copy material data to staging buffer
    stagingBuffer.CopyFrom(&material, sizeof(GPUMaterial));

    // Copy from staging to device-local buffer
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = index * sizeof(GPUMaterial);
    copyRegion.size = sizeof(GPUMaterial);

    vkCmdCopyBuffer(commandBuffer, stagingBuffer.GetBuffer(), m_Buffer.GetBuffer(), 1, &copyRegion);

    EndSingleTimeCommands(commandBuffer);

    stagingBuffer.Destroy();

    return index;
}

void VulkanMaterialBuffer::UpdateMaterial(u32 index, const GPUMaterial& material) {
    if (!m_Context) {
        throw std::runtime_error("VulkanMaterialBuffer::UpdateMaterial called before Init()");
    }

    if (index >= m_MaterialCount) {
        throw std::out_of_range("VulkanMaterialBuffer::UpdateMaterial index out of range");
    }

    // Upload updated material data using staging buffer
    VulkanBuffer stagingBuffer;
    stagingBuffer.Create(
        m_Context,
        sizeof(GPUMaterial),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    stagingBuffer.CopyFrom(&material, sizeof(GPUMaterial));

    VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = index * sizeof(GPUMaterial);
    copyRegion.size = sizeof(GPUMaterial);

    vkCmdCopyBuffer(commandBuffer, stagingBuffer.GetBuffer(), m_Buffer.GetBuffer(), 1, &copyRegion);

    EndSingleTimeCommands(commandBuffer);

    stagingBuffer.Destroy();
}

void VulkanMaterialBuffer::Resize(u32 newCapacity) {
    if (newCapacity <= m_Capacity) {
        return;  // No need to resize
    }

    std::cout << "VulkanMaterialBuffer resizing from " << m_Capacity << " to " << newCapacity << " materials" << std::endl;

    // Create new larger buffer
    VulkanBuffer newBuffer;
    VkDeviceSize newBufferSize = newCapacity * sizeof(GPUMaterial);

    newBuffer.Create(
        m_Context,
        newBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    // Copy existing data to new buffer
    if (m_MaterialCount > 0) {
        VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = m_MaterialCount * sizeof(GPUMaterial);

        vkCmdCopyBuffer(commandBuffer, m_Buffer.GetBuffer(), newBuffer.GetBuffer(), 1, &copyRegion);

        EndSingleTimeCommands(commandBuffer);
    }

    // Destroy old buffer and replace with new
    m_Buffer.Destroy();
    m_Buffer = std::move(newBuffer);
    m_Capacity = newCapacity;
}

VkCommandBuffer VulkanMaterialBuffer::BeginSingleTimeCommands() const {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_Context->GetCommandPool();
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_Context->GetDevice(), &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void VulkanMaterialBuffer::EndSingleTimeCommands(VkCommandBuffer commandBuffer) const {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkQueue graphicsQueue = m_Context->GetGraphicsQueue();
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(m_Context->GetDevice(), m_Context->GetCommandPool(), 1, &commandBuffer);
}
