#include "vulkan_staging_pool.h"

#include "renderer/vulkan_context.h"

#include <stdexcept>
#include <iostream>

VulkanStagingPool::~VulkanStagingPool() {
    Shutdown();
}

void VulkanStagingPool::Init(VulkanContext* context, u64 poolSize) {
    m_Context = context;
    m_PoolSize = poolSize;
    m_CurrentOffset = 0;
    m_OldestPendingOffset = 0;

    // Create mutex for thread safety
    m_Mutex = Platform::CreateMutex();

    // Create a single large host-visible, host-coherent staging buffer
    m_StagingBuffer.Create(
        m_Context,
        m_PoolSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    // Persistently map the buffer for zero-copy writes
    m_MappedData = m_StagingBuffer.Map();

    std::cout << "VulkanStagingPool initialized with " << (m_PoolSize / 1024 / 1024) << " MB" << std::endl;
}

void VulkanStagingPool::Shutdown() {
    if (m_Context) {
        if (m_StagingBuffer.GetBuffer() != VK_NULL_HANDLE) {
            if (m_MappedData) {
                m_StagingBuffer.Unmap();
                m_MappedData = nullptr;
            }
            m_StagingBuffer.Destroy();
        }

        m_Mutex.reset();

        m_Context = nullptr;
        m_PoolSize = 0;
        m_CurrentOffset = 0;
        m_OldestPendingOffset = 0;
        m_PendingAllocations.clear();

        std::cout << "VulkanStagingPool shut down" << std::endl;
    }
}

VulkanStagingPool::StagingAllocation VulkanStagingPool::AcquireStagingBuffer(u64 size, u64 alignment) {
    Platform::Lock(m_Mutex.get());

    // Align the current offset
    u64 alignedOffset = AlignOffset(m_CurrentOffset, alignment);

    // Check if we have enough space
    // Simple linear allocation - if we reach the end, we fail
    // A more sophisticated implementation could wrap around if there's space at the beginning
    if (alignedOffset + size > m_PoolSize) {
        // Try to reclaim space by checking if we can reset to the beginning
        // This requires all pending allocations to be complete
        if (m_PendingAllocations.empty()) {
            // No pending allocations, we can reset
            m_CurrentOffset = 0;
            m_OldestPendingOffset = 0;
            alignedOffset = 0;
        } else {
            Platform::Unlock(m_Mutex.get());
            throw std::runtime_error("VulkanStagingPool out of memory - consider increasing pool size or processing uploads more frequently");
        }
    }

    // Create allocation info
    StagingAllocation allocation;
    allocation.buffer = m_StagingBuffer.GetBuffer();
    allocation.offset = alignedOffset;
    allocation.mappedPtr = static_cast<u8*>(m_MappedData) + alignedOffset;
    allocation.size = size;

    // Advance current offset
    m_CurrentOffset = alignedOffset + size;

    Platform::Unlock(m_Mutex.get());

    return allocation;
}

void VulkanStagingPool::MarkAllocationPending(const StagingAllocation& allocation, u64 timelineValue) {
    Platform::Lock(m_Mutex.get());

    // Add to pending allocations list
    PendingAllocation pending;
    pending.offset = allocation.offset;
    pending.size = allocation.size;
    pending.timelineValue = timelineValue;
    m_PendingAllocations.push_back(pending);

    Platform::Unlock(m_Mutex.get());
}

void VulkanStagingPool::AdvanceFrame(u64 completedTimelineValue) {
    Platform::Lock(m_Mutex.get());

    ReclaimCompletedAllocations(completedTimelineValue);

    Platform::Unlock(m_Mutex.get());
}

void VulkanStagingPool::ReclaimCompletedAllocations(u64 completedTimelineValue) {
    // Remove allocations that have been consumed by the GPU
    auto it = m_PendingAllocations.begin();
    while (it != m_PendingAllocations.end()) {
        if (it->timelineValue <= completedTimelineValue) {
            // This allocation has been consumed
            it = m_PendingAllocations.erase(it);
        } else {
            ++it;
        }
    }

    // Update the oldest pending offset
    if (m_PendingAllocations.empty()) {
        // No pending allocations, we can reset the pool
        m_OldestPendingOffset = m_CurrentOffset;
    } else {
        // Find the oldest pending allocation
        u64 oldestOffset = m_PendingAllocations[0].offset;
        for (const auto& alloc : m_PendingAllocations) {
            if (alloc.offset < oldestOffset) {
                oldestOffset = alloc.offset;
            }
        }
        m_OldestPendingOffset = oldestOffset;
    }
}

u64 VulkanStagingPool::AlignOffset(u64 offset, u64 alignment) {
    if (alignment == 0) {
        return offset;
    }
    return (offset + alignment - 1) & ~(alignment - 1);
}
