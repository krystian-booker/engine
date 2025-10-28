#pragma once

#include "core/types.h"
#include <vulkan/vulkan.h>

#include <array>
#include <mutex>

class VulkanContext;

class VulkanMipmapCompute {
public:
    enum class Variant : u32 {
        Color = 0,
        Normal,
        Roughness,
        Srgb,
        Count
    };

    enum class AlphaMode : u32 {
        Straight = 0,      // Standard alpha blending
        Premultiplied = 1  // Premultiplied alpha (color already multiplied by alpha)
    };

    struct Params {
        VkImage image = VK_NULL_HANDLE;
        VkFormat format = VK_FORMAT_UNDEFINED;
        u32 width = 0;
        u32 height = 0;
        u32 mipLevels = 0;
        u32 baseArrayLayer = 0;
        u32 layerCount = 1;
        Variant variant = Variant::Color;
        AlphaMode alphaMode = AlphaMode::Straight;  // Alpha handling mode for Color variant
        bool hasNormalMap = false;
        VkImage normalImage = VK_NULL_HANDLE;
        VkFormat normalFormat = VK_FORMAT_UNDEFINED;
    };

    VulkanMipmapCompute() = default;
    ~VulkanMipmapCompute();

    void Initialize(VulkanContext* context);
    void Shutdown();
    bool IsInitialized() const { return m_Context != nullptr; }

    // Generates mipmaps for the provided image using compute shaders.
    void Generate(const Params& params);

private:
    VulkanContext* m_Context = nullptr;

    VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
    std::array<VkPipeline, static_cast<size_t>(Variant::Count)> m_Pipelines{};

    // Thread safety for descriptor pool and queue operations
    mutable std::mutex m_Mutex;

    void CreateDescriptorSetLayout();
    void CreatePipelineLayout();
    void CreateDescriptorPool();
    void CreatePipelines();

    VkPipeline GetPipeline(Variant variant) const;

    VkImageView CreateImageView(VkImage image,
                                VkFormat format,
                                u32 baseMipLevel,
                                u32 baseArrayLayer) const;
    VkImageView CreateStorageView(VkImage image,
                                  VkFormat format,
                                  u32 baseMipLevel,
                                  u32 baseArrayLayer) const;

    void DestroyPipelineCache();

    static VkFormat GetStorageCompatibleFormat(VkFormat format);
    static bool IsSrgbFormat(VkFormat format);
};

