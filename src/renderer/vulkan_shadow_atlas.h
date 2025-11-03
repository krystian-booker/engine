#pragma once

#include "core/types.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <optional>

class VulkanContext;

// Shadow atlas region allocated for a specific light/cascade
struct ShadowAtlasRegion {
    u32 x = 0;                  // X offset in atlas texture
    u32 y = 0;                  // Y offset in atlas texture
    u32 width = 0;              // Region width
    u32 height = 0;             // Region height
    u32 arrayLayer = 0;         // Array layer index (for multi-page atlases)
    bool isValid = false;       // Whether this region is currently allocated

    // Normalized UV coordinates for shader access
    f32 uvOffsetX = 0.0f;       // U offset (0-1)
    f32 uvOffsetY = 0.0f;       // V offset (0-1)
    f32 uvScaleX = 1.0f;        // U scale (0-1)
    f32 uvScaleY = 1.0f;        // V scale (0-1)
};

// Shadow atlas allocation handle
struct ShadowAtlasHandle {
    u32 index = UINT32_MAX;     // Index into allocation table
    u32 generation = 0;         // Generation counter for safe invalidation
    bool IsValid() const { return index != UINT32_MAX; }
    void Invalidate() { index = UINT32_MAX; generation = 0; }
};

// Configuration for shadow atlas
struct ShadowAtlasConfig {
    u32 atlasSize = 4096;                           // Atlas texture resolution (width/height)
    u32 numArrayLayers = 4;                         // Number of array layers (pages)
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;    // Depth format
    u32 minAllocationSize = 256;                    // Minimum allocation size (must be power of 2)
    u32 maxAllocationSize = 2048;                   // Maximum allocation size (must be power of 2)
};

// Shadow atlas manager - packs multiple shadow maps into a single texture array
// Uses a simple 2D bin packing algorithm with fixed-size pages
class VulkanShadowAtlas {
public:
    VulkanShadowAtlas() = default;
    ~VulkanShadowAtlas();

    // Initialize atlas with configuration
    void Init(VulkanContext* context, const ShadowAtlasConfig& config = {});

    // Destroy all Vulkan resources
    void Destroy();

    // Allocate a region in the atlas for a shadow map
    // Returns handle to the allocated region, or invalid handle if allocation fails
    ShadowAtlasHandle Allocate(u32 resolution);

    // Free a previously allocated region
    void Free(ShadowAtlasHandle handle);

    // Get region info for a valid handle
    const ShadowAtlasRegion* GetRegion(ShadowAtlasHandle handle) const;

    // Clear all allocations (does not destroy Vulkan resources)
    void ClearAllocations();

    // Accessors
    VkImage GetDepthImage() const { return m_DepthImage; }
    VkImageView GetDepthImageView() const { return m_DepthImageView; }
    VkImageView GetLayerImageView(u32 layer) const;
    VkSampler GetSampler() const { return m_Sampler; }
    VkRenderPass GetRenderPass() const { return m_RenderPass; }
    VkFramebuffer GetFramebuffer(u32 layer) const;
    VkFormat GetDepthFormat() const { return m_Config.depthFormat; }
    u32 GetAtlasSize() const { return m_Config.atlasSize; }
    u32 GetNumLayers() const { return m_Config.numArrayLayers; }

    // Statistics
    u32 GetTotalAllocations() const { return m_TotalAllocations; }
    u32 GetTotalMemoryUsed() const { return m_TotalMemoryUsed; }
    f32 GetFragmentation() const;  // Returns 0-1 indicating fragmentation level

    bool IsInitialized() const { return m_Context != nullptr; }

    // Prevent copying
    VulkanShadowAtlas(const VulkanShadowAtlas&) = delete;
    VulkanShadowAtlas& operator=(const VulkanShadowAtlas&) = delete;

private:
    // Internal allocation node for free list
    struct AllocationNode {
        u32 x = 0;
        u32 y = 0;
        u32 width = 0;
        u32 height = 0;
        u32 layer = 0;
        bool isFree = true;
        u32 generation = 0;  // For handle validation
    };

    // Page in the atlas (one per array layer)
    struct AtlasPage {
        std::vector<AllocationNode> nodes;  // List of allocated/free regions
        u32 freeSpace = 0;                  // Remaining free space in pixels
    };

    void CreateDepthImage();
    void CreateImageViews();
    void CreateRenderPass();
    void CreateFramebuffers();
    void CreateSampler();
    void DestroyResources();

    // Allocation helpers
    std::optional<u32> AllocateInPage(u32 pageIndex, u32 resolution);
    bool TryAllocateInNode(AllocationNode& node, u32 resolution, u32& outX, u32& outY);
    void SplitNode(u32 pageIndex, u32 nodeIndex, u32 allocX, u32 allocY, u32 allocSize);
    u32 FindMemoryType(u32 typeFilter, VkMemoryPropertyFlags properties) const;

    VulkanContext* m_Context = nullptr;
    ShadowAtlasConfig m_Config;

    // Vulkan resources
    VkImage m_DepthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_DepthImageMemory = VK_NULL_HANDLE;
    VkImageView m_DepthImageView = VK_NULL_HANDLE;          // Full array view for sampling
    std::vector<VkImageView> m_LayerImageViews;             // Per-layer views for rendering
    VkSampler m_Sampler = VK_NULL_HANDLE;
    VkRenderPass m_RenderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_Framebuffers;              // One per layer

    // Allocation tracking
    std::vector<AtlasPage> m_Pages;                         // One page per array layer
    std::vector<AllocationNode> m_AllocationTable;          // Table of all allocations (indexed by handle)
    std::vector<ShadowAtlasRegion> m_Regions;               // Region data (indexed by handle)
    std::vector<u32> m_FreeHandles;                         // Pool of freed handles for reuse

    // Statistics
    u32 m_TotalAllocations = 0;
    u32 m_TotalMemoryUsed = 0;
};
