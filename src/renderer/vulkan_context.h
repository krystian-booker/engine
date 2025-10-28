#pragma once

#include "core/types.h"
#include "renderer/vulkan_mipmap_compute.h"

#include <vulkan/vulkan.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

class Window;

class VulkanContext {
public:
    VulkanContext() = default;
    ~VulkanContext();

    void Init(Window* window);
    void Shutdown();

    VkInstance GetInstance() const { return m_Instance; }
    VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
    VkDevice GetDevice() const { return m_Device; }
    VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
    VkQueue GetPresentQueue() const { return m_PresentQueue; }
    VkSurfaceKHR GetSurface() const { return m_Surface; }
    VkCommandPool GetCommandPool() const { return m_CommandPool; }
    VulkanMipmapCompute* GetMipmapCompute();

    u32 GetGraphicsQueueFamily() const { return m_GraphicsQueueFamily; }
    u32 GetPresentQueueFamily() const { return m_PresentQueueFamily; }

    // Timeline semaphore for async transfers
    VkSemaphore GetTransferTimelineSemaphore() const { return m_TransferTimelineSemaphore; }
    u64 GetNextTransferTimelineValue() { return ++m_TransferTimelineValue; }
    u64 GetCurrentTransferTimelineValue() const { return m_TransferTimelineValue.load(); }

    // Format capability queries
    bool SupportsLinearBlit(VkFormat format) const;
    bool SupportsColorAttachment(VkFormat format) const;
    bool SupportsDepthStencilAttachment(VkFormat format) const;
    bool SupportsTransferSrc(VkFormat format) const;
    bool SupportsTransferDst(VkFormat format) const;
    bool SupportsSampledImage(VkFormat format) const;
    bool SupportsStorageImage(VkFormat format) const;
    const VkFormatProperties* GetFormatProperties(VkFormat format) const;

private:
    VkInstance m_Instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;

    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;

    VkDevice m_Device = VK_NULL_HANDLE;

    VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
    VkQueue m_PresentQueue = VK_NULL_HANDLE;
    u32 m_GraphicsQueueFamily = 0;
    u32 m_PresentQueueFamily = 0;
    VkCommandPool m_CommandPool = VK_NULL_HANDLE;

    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;

    // Timeline semaphore for async transfers
    VkSemaphore m_TransferTimelineSemaphore = VK_NULL_HANDLE;
    std::atomic<u64> m_TransferTimelineValue{0};

    // Format capabilities cache
    mutable std::unordered_map<VkFormat, VkFormatProperties> m_FormatCapabilities;

    std::unique_ptr<VulkanMipmapCompute> m_MipmapCompute;

    const std::vector<const char*> m_ValidationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

#ifdef _DEBUG
    const bool m_EnableValidationLayers = true;
#else
    const bool m_EnableValidationLayers = false;
#endif

    void CreateInstance();
    void SetupDebugMessenger();
    void CreateSurface(Window* window);
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CacheFormatCapabilities();

    bool CheckValidationLayerSupport();
    std::vector<const char*> GetRequiredExtensions();

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);
};
