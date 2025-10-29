#pragma once

#include "core/resource_manager.h"
#include "core/material_data.h"
#include "core/resource_handle.h"
#include <vector>

// Forward declarations
class VulkanContext;
class VulkanMaterialBuffer;

// Material resource manager (singleton)
class MaterialManager : public ResourceManager<MaterialData, MaterialHandle> {
public:
    // Singleton access
    static MaterialManager& Instance() {
        static MaterialManager instance;
        return instance;
    }

    // Initialize GPU buffer for materials (must be called after VulkanContext is created)
    void InitGPUBuffer(VulkanContext* context);

    // Shutdown GPU buffer
    void ShutdownGPUBuffer();

    // Load material from JSON file
    MaterialHandle Load(const std::string& filepath);

    // Create default material (white albedo, flat normal, mid roughness)
    MaterialHandle CreateDefaultMaterial();

    // Get default fallback material
    MaterialHandle GetDefaultMaterial() const { return m_DefaultMaterial; }

    // Upload material data to GPU SSBO
    // Returns GPU material index (slot in SSBO)
    u32 UploadMaterialToGPU(const MaterialData& material);

    // Update existing GPU material data
    void UpdateMaterialOnGPU(u32 gpuIndex, const MaterialData& material);

    // Get GPU material buffer
    VulkanMaterialBuffer* GetGPUBuffer() { return m_GPUBuffer.get(); }

    // Ensure material is uploaded to GPU with descriptor caching (Phase 6.4)
    // Returns GPU material index, updates descriptor set if needed
    u32 EnsureMaterial(MaterialHandle handle);

    // Invalidate material descriptor cache (marks as dirty for rebuild)
    void InvalidateMaterial(MaterialHandle handle);

    // Invalidate all materials using a specific texture (for hot-reload)
    void InvalidateMaterialsUsingTexture(TextureHandle texHandle);

protected:
    // Override ResourceManager::LoadResource to parse JSON
    std::unique_ptr<MaterialData> LoadResource(const std::string& filepath) override;

private:
    MaterialManager();
    ~MaterialManager();

    // Prevent copying
    MaterialManager(const MaterialManager&) = delete;
    MaterialManager& operator=(const MaterialManager&) = delete;

    // Parse JSON material file
    std::unique_ptr<MaterialData> LoadMaterialFromJSON(const std::string& filepath);

    // Infer TextureLoadOptions from slot name
    struct TextureLoadOptions InferTextureOptions(const std::string& slotName) const;

    // Cache for default material
    MaterialHandle m_DefaultMaterial;

    // GPU material buffer (SSBO)
    std::unique_ptr<VulkanMaterialBuffer> m_GPUBuffer;
    VulkanContext* m_VulkanContext = nullptr;

    // Material index allocator (sequential assignment)
    u32 m_NextGPUMaterialIndex = 0;
};
