#pragma once

#include "core/types.h"
#include "platform/platform.h"
#include "renderer/vulkan_buffer.h"

#include <vulkan/vulkan.h>

#include <vector>

class VulkanContext;

// Ring buffer allocator for staging memory used in async texture uploads.
// Allocations are tracked with timeline semaphore values and recycled after GPU consumption.
class VulkanStagingPool {
public:
    VulkanStagingPool() = default;
    ~VulkanStagingPool();

    // Allocation result containing buffer, offset, and mapped pointer
    struct StagingAllocation {
        VkBuffer buffer = VK_NULL_HANDLE;
        u64 offset = 0;
        void* mappedPtr = nullptr;
        u64 size = 0;
    };

    // Initialize the staging pool with a large backing buffer
    // poolSize: Total size of the staging buffer (default 64MB)
    void Init(VulkanContext* context, u64 poolSize = 64 * 1024 * 1024);

    // Shutdown and free all resources
    void Shutdown();

    // Acquire a staging buffer allocation
    // size: Number of bytes to allocate
    // alignment: Alignment requirement (default 16 bytes)
    // Returns: Allocation info with buffer handle, offset, and mapped pointer
    // Thread-safe
    StagingAllocation AcquireStagingBuffer(u64 size, u64 alignment = 16);

    // Mark an allocation as pending with a timeline value
    // This should be called after submitting a command buffer that uses the allocation
    // allocation: The allocation to track
    // timelineValue: The timeline semaphore value that will be signaled when GPU is done
    // Thread-safe
    void MarkAllocationPending(const StagingAllocation& allocation, u64 timelineValue);

    // Advance the frame and reclaim memory from completed transfers
    // completedTimelineValue: Timeline value that has been completed by GPU
    // Call this once per frame after checking timeline semaphore
    void AdvanceFrame(u64 completedTimelineValue);

    // Get total pool size
    u64 GetPoolSize() const { return m_PoolSize; }

    // Get currently allocated size
    u64 GetAllocatedSize() const { return m_CurrentOffset - m_OldestPendingOffset; }

private:
    VulkanContext* m_Context = nullptr;
    VulkanBuffer m_StagingBuffer;
    void* m_MappedData = nullptr;
    u64 m_PoolSize = 0;
    u64 m_CurrentOffset = 0;
    u64 m_OldestPendingOffset = 0;

    // Track pending allocations with their timeline values
    struct PendingAllocation {
        u64 offset;
        u64 size;
        u64 timelineValue;
    };
    std::vector<PendingAllocation> m_PendingAllocations;

    // Thread safety
    Platform::MutexPtr m_Mutex;

    // Helper to reclaim space from completed allocations
    void ReclaimCompletedAllocations(u64 completedTimelineValue);

    // Align offset to alignment boundary
    static u64 AlignOffset(u64 offset, u64 alignment);
};
