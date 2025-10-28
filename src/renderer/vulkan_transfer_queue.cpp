#include "vulkan_transfer_queue.h"

#include "renderer/vulkan_context.h"

#include <stdexcept>
#include <iostream>

VulkanTransferQueue::~VulkanTransferQueue() {
    Shutdown();
}

void VulkanTransferQueue::Init(VulkanContext* context, u32 framesInFlight) {
    m_Context = context;
    m_FramesInFlight = framesInFlight;

    // Create per-frame command pools
    m_CommandPools.resize(m_FramesInFlight);

    for (u32 i = 0; i < m_FramesInFlight; ++i) {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        poolInfo.queueFamilyIndex = m_Context->GetGraphicsQueueFamily(); // Using graphics queue for transfers

        if (vkCreateCommandPool(m_Context->GetDevice(), &poolInfo, nullptr, &m_CommandPools[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create transfer command pool");
        }
    }

    std::cout << "VulkanTransferQueue initialized with " << m_FramesInFlight << " frames in flight" << std::endl;
}

void VulkanTransferQueue::Shutdown() {
    if (m_Context) {
        VkDevice device = m_Context->GetDevice();

        // Free active command buffers
        if (!m_ActiveCommandBuffers.empty() && m_CurrentFrameIndex < m_CommandPools.size()) {
            vkFreeCommandBuffers(
                device,
                m_CommandPools[m_CurrentFrameIndex],
                static_cast<u32>(m_ActiveCommandBuffers.size()),
                m_ActiveCommandBuffers.data()
            );
            m_ActiveCommandBuffers.clear();
        }

        // Destroy command pools
        for (VkCommandPool pool : m_CommandPools) {
            if (pool != VK_NULL_HANDLE) {
                vkDestroyCommandPool(device, pool, nullptr);
            }
        }
        m_CommandPools.clear();

        m_Context = nullptr;
        m_FramesInFlight = 0;

        std::cout << "VulkanTransferQueue shut down" << std::endl;
    }
}

VkCommandBuffer VulkanTransferQueue::BeginTransferCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_CommandPools[m_CurrentFrameIndex];
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(m_Context->GetDevice(), &allocInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate transfer command buffer");
    }

    // Track this command buffer so we can free it later
    m_ActiveCommandBuffers.push_back(commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording transfer command buffer");
    }

    return commandBuffer;
}

u64 VulkanTransferQueue::SubmitTransferCommands(VkCommandBuffer cmd) {
    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record transfer command buffer");
    }

    // Get the next timeline value
    u64 timelineValue = m_Context->GetNextTransferTimelineValue();

    // Submit with timeline semaphore signal
    VkTimelineSemaphoreSubmitInfo timelineInfo{};
    timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    timelineInfo.pNext = nullptr;
    timelineInfo.waitSemaphoreValueCount = 0;
    timelineInfo.pWaitSemaphoreValues = nullptr;
    timelineInfo.signalSemaphoreValueCount = 1;
    timelineInfo.pSignalSemaphoreValues = &timelineValue;

    VkSemaphore timelineSemaphore = m_Context->GetTransferTimelineSemaphore();

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = &timelineInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &timelineSemaphore;

    if (vkQueueSubmit(m_Context->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit transfer command buffer");
    }

    return timelineValue;
}

bool VulkanTransferQueue::IsTransferComplete(u64 timelineValue) const {
    VkSemaphore timelineSemaphore = m_Context->GetTransferTimelineSemaphore();

    u64 currentValue = 0;
    if (vkGetSemaphoreCounterValue(m_Context->GetDevice(), timelineSemaphore, &currentValue) != VK_SUCCESS) {
        return false;
    }

    return currentValue >= timelineValue;
}

void VulkanTransferQueue::ResetForFrame(u32 frameIndex) {
    m_CurrentFrameIndex = frameIndex;

    // Free all command buffers allocated for this frame
    if (!m_ActiveCommandBuffers.empty()) {
        vkFreeCommandBuffers(
            m_Context->GetDevice(),
            m_CommandPools[m_CurrentFrameIndex],
            static_cast<u32>(m_ActiveCommandBuffers.size()),
            m_ActiveCommandBuffers.data()
        );
        m_ActiveCommandBuffers.clear();
    }

    // Reset the command pool to recycle command buffer memory
    vkResetCommandPool(m_Context->GetDevice(), m_CommandPools[m_CurrentFrameIndex], 0);
}
