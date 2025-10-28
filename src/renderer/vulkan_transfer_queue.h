#pragma once

#include "core/types.h"

#include <vulkan/vulkan.h>

#include <vector>

class VulkanContext;

// Manages async command buffer submission for texture/buffer uploads.
// Uses per-frame command pools and timeline semaphores for completion tracking.
class VulkanTransferQueue {
public:
    VulkanTransferQueue() = default;
    ~VulkanTransferQueue();

    // Initialize with context and number of frames in flight
    void Init(VulkanContext* context, u32 framesInFlight = 2);

    // Shutdown and free all resources
    void Shutdown();

    // Begin recording transfer commands
    // Returns a command buffer ready for recording
    // Not thread-safe - should be called from main thread
    VkCommandBuffer BeginTransferCommands();

    // Submit recorded transfer commands with timeline semaphore signaling
    // cmd: Command buffer returned from BeginTransferCommands()
    // Returns: Timeline semaphore value that will be signaled when complete
    // Not thread-safe - should be called from main thread
    u64 SubmitTransferCommands(VkCommandBuffer cmd);

    // Check if a transfer with the given timeline value has completed
    // timelineValue: Value returned from SubmitTransferCommands()
    // Returns: true if the transfer is complete
    bool IsTransferComplete(u64 timelineValue) const;

    // Reset command pool for the current frame
    // frameIndex: Current frame index (0 or 1 for double buffering)
    // Call this at the beginning of each frame
    void ResetForFrame(u32 frameIndex);

private:
    VulkanContext* m_Context = nullptr;
    u32 m_FramesInFlight = 0;

    // Per-frame command pools for transfer operations
    std::vector<VkCommandPool> m_CommandPools;

    // Command buffers allocated for current frame
    std::vector<VkCommandBuffer> m_ActiveCommandBuffers;
    u32 m_CurrentFrameIndex = 0;
};
