#pragma once

#include "core/types.h"
#include "renderer/vulkan_buffer.h"

#include <vulkan/vulkan.h>

class VulkanContext;
struct MeshData;

class VulkanMesh {
public:
    VulkanMesh() = default;
    ~VulkanMesh();

    void Create(VulkanContext* context, const MeshData* meshData);
    void Destroy();

    void Bind(VkCommandBuffer commandBuffer) const;
    void Draw(VkCommandBuffer commandBuffer) const;

    u32 GetIndexCount() const { return m_IndexCount; }
    VkIndexType GetIndexType() const { return m_IndexType; }
    bool IsValid() const {
        return m_VertexBuffer.GetBuffer() != VK_NULL_HANDLE &&
            m_IndexBuffer.GetBuffer() != VK_NULL_HANDLE;
    }

private:
    VulkanContext* m_Context = nullptr;

    VulkanBuffer m_VertexBuffer;
    VulkanBuffer m_IndexBuffer;

    u32 m_VertexCount = 0;
    u32 m_IndexCount = 0;
    VkIndexType m_IndexType = VK_INDEX_TYPE_UINT32;

    void CreateVertexBuffer(const MeshData* meshData);
    void CreateIndexBuffer(const MeshData* meshData);
    void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
};
