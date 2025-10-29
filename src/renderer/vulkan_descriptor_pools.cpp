#include "vulkan_descriptor_pools.h"
#include "vulkan_context.h"
#include "core/types.h"
#include <iostream>

void VulkanDescriptorPools::Init(VulkanContext* context, u32 framesInFlight) {
    m_Context = context;
    m_FramesInFlight = framesInFlight;

    // Initialize transient pools (one set per frame)
    m_TransientPools.resize(framesInFlight);

    // Create initial persistent pool
    VkDescriptorPool persistentPool = CreateNewPool(PoolType::Persistent, INITIAL_PERSISTENT_POOL_SIZE);
    if (persistentPool != VK_NULL_HANDLE) {
        m_PersistentPools.push_back({
            persistentPool,
            0,  // allocatedSets
            INITIAL_PERSISTENT_POOL_SIZE,
            {}  // freeList
        });
    }

    // Create initial transient pools (one per frame)
    for (u32 i = 0; i < framesInFlight; ++i) {
        VkDescriptorPool transientPool = CreateNewPool(PoolType::Transient, INITIAL_TRANSIENT_POOL_SIZE);
        if (transientPool != VK_NULL_HANDLE) {
            m_TransientPools[i].push_back({
                transientPool,
                0,  // allocatedSets
                INITIAL_TRANSIENT_POOL_SIZE,
                {}  // freeList
            });
        }
    }

    std::cout << "VulkanDescriptorPools initialized: " << framesInFlight << " frames in flight" << std::endl;
}

void VulkanDescriptorPools::Cleanup() {
    VkDevice device = m_Context->GetDevice();

    // Destroy all persistent pools
    for (auto& poolInfo : m_PersistentPools) {
        if (poolInfo.pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, poolInfo.pool, nullptr);
        }
    }
    m_PersistentPools.clear();

    // Destroy all transient pools
    for (auto& framePools : m_TransientPools) {
        for (auto& poolInfo : framePools) {
            if (poolInfo.pool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(device, poolInfo.pool, nullptr);
            }
        }
    }
    m_TransientPools.clear();

    m_SetToPoolMap.clear();

    std::cout << "VulkanDescriptorPools cleaned up" << std::endl;
}

VkDescriptorPool VulkanDescriptorPools::CreateNewPool(PoolType type, u32 setsPerPool) {
    VkDevice device = m_Context->GetDevice();

    // Define pool sizes based on pool type
    std::vector<VkDescriptorPoolSize> poolSizes;

    if (type == PoolType::Transient) {
        // Transient pools: Only uniform buffers for per-frame data
        poolSizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, setsPerPool});
    } else {
        // Persistent pools: Material SSBO + Bindless texture array
        poolSizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, setsPerPool});
        poolSizes.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4096 * setsPerPool});
    }

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = setsPerPool;

    // Transient pools can be reset, persistent pools support UPDATE_AFTER_BIND
    if (type == PoolType::Transient) {
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    } else {
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT |
                         VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    }

    VkDescriptorPool pool;
    VkResult result = vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool);

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create descriptor pool! VkResult: " << result << std::endl;
        return VK_NULL_HANDLE;
    }

    std::cout << "Created new " << (type == PoolType::Transient ? "transient" : "persistent")
              << " descriptor pool with capacity: " << setsPerPool << std::endl;

    return pool;
}

VkDescriptorSet VulkanDescriptorPools::AllocateDescriptorSet(
    VkDescriptorSetLayout layout,
    PoolType type,
    u32 frameIndex
) {
    std::lock_guard<std::mutex> lock(m_AllocationMutex);

    // Select appropriate pool list
    std::vector<PoolInfo>* poolList = nullptr;
    if (type == PoolType::Transient) {
        if (frameIndex >= m_FramesInFlight) {
            std::cerr << "Invalid frame index: " << frameIndex << std::endl;
            return VK_NULL_HANDLE;
        }
        poolList = &m_TransientPools[frameIndex];
    } else {
        poolList = &m_PersistentPools;
    }

    // Step 1: Check freelists first (recycling)
    for (auto& poolInfo : *poolList) {
        if (!poolInfo.freeList.empty()) {
            VkDescriptorSet recycled = poolInfo.freeList.back();
            poolInfo.freeList.pop_back();
            return recycled;
        }
    }

    // Step 2: Try allocating from existing pools
    VkDescriptorSet set = TryAllocateFromExistingPools(*poolList, layout);
    if (set != VK_NULL_HANDLE) {
        return set;
    }

    // Step 3: All pools exhausted - create new pool with 2x growth
    u32 newPoolSize = poolList->empty() ?
        (type == PoolType::Transient ? INITIAL_TRANSIENT_POOL_SIZE : INITIAL_PERSISTENT_POOL_SIZE) :
        poolList->back().maxSets * 2;  // 2x growth factor

    VkDescriptorPool newPool = CreateNewPool(type, newPoolSize);
    if (newPool == VK_NULL_HANDLE) {
        std::cerr << "Failed to create new descriptor pool during growth!" << std::endl;
        return VK_NULL_HANDLE;
    }

    // Add new pool to list
    poolList->push_back({
        newPool,
        0,  // allocatedSets
        newPoolSize,
        {}  // freeList
    });

    // Retry allocation with new pool
    set = TryAllocateFromExistingPools(*poolList, layout);
    if (set == VK_NULL_HANDLE) {
        std::cerr << "Failed to allocate from newly created pool!" << std::endl;
    }

    return set;
}

VkDescriptorSet VulkanDescriptorPools::TryAllocateFromExistingPools(
    std::vector<PoolInfo>& pools,
    VkDescriptorSetLayout layout
) {
    VkDevice device = m_Context->GetDevice();

    // Try each pool in order
    for (auto& poolInfo : pools) {
        if (poolInfo.allocatedSets >= poolInfo.maxSets) {
            continue;  // Pool is full
        }

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = poolInfo.pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;

        VkDescriptorSet set;
        VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &set);

        if (result == VK_SUCCESS) {
            poolInfo.allocatedSets++;
            m_SetToPoolMap[set] = poolInfo.pool;  // Track ownership
            return set;
        }

        if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
            // Pool reported as full, try next pool
            continue;
        }

        // Other error - log and continue
        std::cerr << "Descriptor set allocation failed with VkResult: " << result << std::endl;
    }

    return VK_NULL_HANDLE;  // All pools exhausted
}

void VulkanDescriptorPools::FreeDescriptorSet(VkDescriptorSet set, PoolType type) {
    if (set == VK_NULL_HANDLE) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_AllocationMutex);

    // Find which pool owns this set
    auto it = m_SetToPoolMap.find(set);
    if (it == m_SetToPoolMap.end()) {
        std::cerr << "Attempted to free unknown descriptor set!" << std::endl;
        return;
    }

    // Select appropriate pool list
    std::vector<PoolInfo>* poolList = nullptr;
    if (type == PoolType::Transient) {
        // Transient sets shouldn't be individually freed - they're reset
        std::cerr << "Warning: Freeing transient descriptor set individually. Use ResetTransientPool instead." << std::endl;
        return;
    } else {
        poolList = &m_PersistentPools;
    }

    // Find pool and add to freelist
    PoolInfo* poolInfo = FindPoolOwner(*poolList, set);
    if (poolInfo) {
        poolInfo->freeList.push_back(set);
        poolInfo->allocatedSets--;
    } else {
        std::cerr << "Could not find pool owner for descriptor set!" << std::endl;
    }

    // Keep set in map for debugging, but mark as freed
    // m_SetToPoolMap.erase(it);  // Optionally remove from tracking
}

VulkanDescriptorPools::PoolInfo* VulkanDescriptorPools::FindPoolOwner(
    std::vector<PoolInfo>& pools,
    VkDescriptorSet set
) {
    auto it = m_SetToPoolMap.find(set);
    if (it == m_SetToPoolMap.end()) {
        return nullptr;
    }

    VkDescriptorPool ownerPool = it->second;

    for (auto& poolInfo : pools) {
        if (poolInfo.pool == ownerPool) {
            return &poolInfo;
        }
    }

    return nullptr;
}

void VulkanDescriptorPools::ResetTransientPool(u32 frameIndex) {
    if (frameIndex >= m_FramesInFlight) {
        std::cerr << "Invalid frame index for transient pool reset: " << frameIndex << std::endl;
        return;
    }

    VkDevice device = m_Context->GetDevice();

    // Reset all pools for this frame
    for (auto& poolInfo : m_TransientPools[frameIndex]) {
        if (poolInfo.pool != VK_NULL_HANDLE) {
            vkResetDescriptorPool(device, poolInfo.pool, 0);
            poolInfo.allocatedSets = 0;
            poolInfo.freeList.clear();
        }
    }

    // Remove transient sets from tracking map
    for (auto it = m_SetToPoolMap.begin(); it != m_SetToPoolMap.end(); ) {
        // Check if this set belongs to any of this frame's pools
        bool belongsToThisFrame = false;
        for (const auto& poolInfo : m_TransientPools[frameIndex]) {
            if (it->second == poolInfo.pool) {
                belongsToThisFrame = true;
                break;
            }
        }

        if (belongsToThisFrame) {
            it = m_SetToPoolMap.erase(it);
        } else {
            ++it;
        }
    }
}

VulkanDescriptorPools::PoolStats VulkanDescriptorPools::GetStats(PoolType type) const {
    std::lock_guard<std::mutex> lock(m_AllocationMutex);

    PoolStats stats{};

    if (type == PoolType::Persistent) {
        stats.totalPools = static_cast<u32>(m_PersistentPools.size());
        for (const auto& poolInfo : m_PersistentPools) {
            stats.totalAllocatedSets += poolInfo.allocatedSets;
            stats.totalMaxSets += poolInfo.maxSets;
            stats.recycledSets += static_cast<u32>(poolInfo.freeList.size());
        }
    } else {
        for (const auto& framePools : m_TransientPools) {
            stats.totalPools += static_cast<u32>(framePools.size());
            for (const auto& poolInfo : framePools) {
                stats.totalAllocatedSets += poolInfo.allocatedSets;
                stats.totalMaxSets += poolInfo.maxSets;
                stats.recycledSets += static_cast<u32>(poolInfo.freeList.size());
            }
        }
    }

    return stats;
}
