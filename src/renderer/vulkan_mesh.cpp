#include "renderer/vulkan_mesh.h"

#include "renderer/vulkan_context.h"
#include "renderer/vertex.h"
#include "resources/mesh_manager.h"

#include <stdexcept>

VulkanMesh::~VulkanMesh() {
    Destroy();
}

void VulkanMesh::Create(VulkanContext* context, const MeshData* meshData) {
    if (!context) {
        throw std::invalid_argument("VulkanMesh::Create requires valid context");
    }

    if (!meshData) {
        throw std::invalid_argument("VulkanMesh::Create requires valid mesh data");
    }

    if (meshData->vertices.empty()) {
        throw std::invalid_argument("VulkanMesh::Create requires vertex data");
    }

    if (meshData->indices.empty()) {
        throw std::invalid_argument("VulkanMesh::Create requires index data");
    }

    Destroy();

    m_Context = context;
    m_VertexCount = meshData->vertexCount != 0 ? meshData->vertexCount
                                               : static_cast<u32>(meshData->vertices.size());
    m_IndexCount = meshData->indexCount != 0 ? meshData->indexCount
                                             : static_cast<u32>(meshData->indices.size());
    m_IndexType = VK_INDEX_TYPE_UINT32;

    CreateVertexBuffer(meshData);
    CreateIndexBuffer(meshData);
}

void VulkanMesh::Destroy() {
    m_VertexBuffer.Destroy();
    m_IndexBuffer.Destroy();

    m_Context = nullptr;
    m_VertexCount = 0;
    m_IndexCount = 0;
    m_IndexType = VK_INDEX_TYPE_UINT32;
}

void VulkanMesh::Bind(VkCommandBuffer commandBuffer) const {
    if (!IsValid()) {
        throw std::runtime_error("VulkanMesh::Bind called on invalid mesh");
    }

    VkBuffer vertexBuffers[] = { m_VertexBuffer.GetBuffer() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer.GetBuffer(), 0, m_IndexType);
}

void VulkanMesh::Draw(VkCommandBuffer commandBuffer) const {
    if (!IsValid()) {
        throw std::runtime_error("VulkanMesh::Draw called on invalid mesh");
    }

    if (m_IndexCount == 0) {
        return;
    }

    vkCmdDrawIndexed(commandBuffer, m_IndexCount, 1, 0, 0, 0);
}

void VulkanMesh::CreateVertexBuffer(const MeshData* meshData) {
    VkDeviceSize bufferSize = sizeof(Vertex) * meshData->vertices.size();
    if (bufferSize == 0) {
        throw std::runtime_error("VulkanMesh::CreateVertexBuffer requires non-empty vertices");
    }

    VulkanBuffer staging;
    staging.Create(
        m_Context,
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    staging.CopyFrom(meshData->vertices.data(), bufferSize);

    m_VertexBuffer.Create(
        m_Context,
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    CopyBuffer(staging.GetBuffer(), m_VertexBuffer.GetBuffer(), bufferSize);
    staging.Destroy();
}

void VulkanMesh::CreateIndexBuffer(const MeshData* meshData) {
    VkDeviceSize bufferSize = sizeof(u32) * meshData->indices.size();
    if (bufferSize == 0) {
        throw std::runtime_error("VulkanMesh::CreateIndexBuffer requires non-empty indices");
    }

    VulkanBuffer staging;
    staging.Create(
        m_Context,
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    staging.CopyFrom(meshData->indices.data(), bufferSize);

    m_IndexBuffer.Create(
        m_Context,
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    CopyBuffer(staging.GetBuffer(), m_IndexBuffer.GetBuffer(), bufferSize);
    staging.Destroy();
}

void VulkanMesh::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    if (!m_Context) {
        throw std::runtime_error("VulkanMesh::CopyBuffer requires valid context");
    }

    if (size == 0) {
        return;
    }

    VkDevice device = m_Context->GetDevice();
    VkCommandPool commandPool = m_Context->GetCommandPool();
    if (commandPool == VK_NULL_HANDLE) {
        throw std::runtime_error("VulkanMesh::CopyBuffer missing command pool");
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("VulkanMesh::CopyBuffer failed to allocate command buffer");
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        throw std::runtime_error("VulkanMesh::CopyBuffer failed to begin command buffer");
    }

    VkBufferCopy copyRegion{};
    copyRegion.size = size;

    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        throw std::runtime_error("VulkanMesh::CopyBuffer failed to record command buffer");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkQueue graphicsQueue = m_Context->GetGraphicsQueue();
    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        throw std::runtime_error("VulkanMesh::CopyBuffer failed to submit command buffer");
    }

    vkQueueWaitIdle(graphicsQueue);
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}
