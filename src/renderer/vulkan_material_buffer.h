#pragma once

#include "core/types.h"
#include "renderer/material_buffer.h"
#include "renderer/vulkan_buffer.h"
#include <vulkan/vulkan.h>
#include <vector>

class VulkanContext;

// Manages GPU storage buffer for material data (SSBO)
class VulkanMaterialBuffer {
public:
    VulkanMaterialBuffer() = default;
    ~VulkanMaterialBuffer();

    // Initialize with initial capacity
    void Init(VulkanContext* context, u32 initialCapacity = 256);

    // Shutdown and free resources
    void Shutdown();

    // Upload material data to GPU
    // Returns the index where the material was stored
    u32 UploadMaterial(const GPUMaterial& material);

    // Update existing material data at index
    void UpdateMaterial(u32 index, const GPUMaterial& material);

    // Get GPU buffer handle
    VkBuffer GetBuffer() const { return m_Buffer.GetBuffer(); }

    // Get buffer size in bytes
    VkDeviceSize GetBufferSize() const { return m_Buffer.GetSize(); }

    // Get material capacity
    u32 GetCapacity() const { return m_Capacity; }

    // Get current material count
    u32 GetMaterialCount() const { return m_MaterialCount; }

private:
    VulkanContext* m_Context = nullptr;
    VulkanBuffer m_Buffer;  // Device-local storage buffer

    u32 m_Capacity = 0;       // Max number of materials
    u32 m_MaterialCount = 0;  // Current number of materials

    // Resize buffer to accommodate more materials
    void Resize(u32 newCapacity);

    // Helper functions for command buffer submission
    VkCommandBuffer BeginSingleTimeCommands() const;
    void EndSingleTimeCommands(VkCommandBuffer commandBuffer) const;
};
