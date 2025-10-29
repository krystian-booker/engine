#pragma once

#include "core/types.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <mutex>
#include <unordered_map>

// Forward declaration
class VulkanContext;

/**
 * VulkanDescriptorPools - Advanced descriptor pool management
 *
 * Features:
 * - Dynamic pool allocation with automatic growth (2x factor)
 * - Descriptor set recycling via freelists
 * - Transient/persistent pool separation
 * - Thread-safe allocation for async loading
 */
class VulkanDescriptorPools {
public:
    enum class PoolType {
        Transient,   // Reset each frame (per-frame UBOs, camera data)
        Persistent   // Long-lived, recycled (materials, textures)
    };

    VulkanDescriptorPools() = default;
    ~VulkanDescriptorPools() = default;

    // Initialize pools with frames in flight count
    void Init(VulkanContext* context, u32 framesInFlight);

    // Shutdown and cleanup all pools
    void Cleanup();

    // Allocate a descriptor set from appropriate pool
    // Returns VK_NULL_HANDLE on failure
    // variableDescriptorCount: optional descriptor count for variable descriptor count bindings
    VkDescriptorSet AllocateDescriptorSet(
        VkDescriptorSetLayout layout,
        PoolType type,
        u32 frameIndex = 0,
        u32 variableDescriptorCount = 0
    );

    // Free a descriptor set (adds to freelist for recycling)
    void FreeDescriptorSet(VkDescriptorSet set, PoolType type);

    // Reset transient pool for a specific frame (call at BeginFrame)
    void ResetTransientPool(u32 frameIndex);

    // Get statistics for debugging/profiling
    struct PoolStats {
        u32 totalPools;
        u32 totalAllocatedSets;
        u32 totalMaxSets;
        u32 recycledSets;
    };
    PoolStats GetStats(PoolType type) const;

private:
    // Pool information tracking
    struct PoolInfo {
        VkDescriptorPool pool{VK_NULL_HANDLE};
        u32 allocatedSets{0};          // Currently allocated from this pool
        u32 maxSets{0};                // Maximum capacity
        std::vector<VkDescriptorSet> freeList;  // Recycled descriptor sets
    };

    // Create a new descriptor pool with specified capacity
    VkDescriptorPool CreateNewPool(PoolType type, u32 setsPerPool);

    // Attempt to allocate from an existing pool (returns VK_NULL_HANDLE if all exhausted)
    VkDescriptorSet TryAllocateFromExistingPools(
        std::vector<PoolInfo>& pools,
        VkDescriptorSetLayout layout,
        u32 variableDescriptorCount = 0
    );

    // Find which pool owns a descriptor set
    PoolInfo* FindPoolOwner(
        std::vector<PoolInfo>& pools,
        VkDescriptorSet set
    );

    // Context reference
    VulkanContext* m_Context{nullptr};
    u32 m_FramesInFlight{0};

    // Persistent pools (shared across all frames)
    std::vector<PoolInfo> m_PersistentPools;

    // Transient pools (per-frame, reset each frame)
    // m_TransientPools[frameIndex][poolIndex]
    std::vector<std::vector<PoolInfo>> m_TransientPools;

    // Thread safety for allocation/free operations
    mutable std::mutex m_AllocationMutex;

    // Pool size configurations
    static constexpr u32 INITIAL_TRANSIENT_POOL_SIZE = 64;
    static constexpr u32 INITIAL_PERSISTENT_POOL_SIZE = 256;

    // Mapping from descriptor set to owning pool (for FreeDescriptorSet)
    std::unordered_map<VkDescriptorSet, VkDescriptorPool> m_SetToPoolMap;
};
