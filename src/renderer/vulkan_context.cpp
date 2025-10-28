#include "vulkan_context.h"

#include "platform/window.h"
#include "renderer/vulkan_mipmap_compute.h"

#include <GLFW/glfw3.h>

#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>

VulkanContext::~VulkanContext() {
    Shutdown();
}

void VulkanContext::Init(Window* window) {
    CreateInstance();
    SetupDebugMessenger();
    CreateSurface(window);
    PickPhysicalDevice();
    CacheFormatCapabilities();
    CreateLogicalDevice();

    std::cout << "Vulkan context initialized" << std::endl;
}

void VulkanContext::Shutdown() {
    if (m_Device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_Device);

        if (m_MipmapCompute) {
            m_MipmapCompute->Shutdown();
            m_MipmapCompute.reset();
        }

        if (m_CommandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
            m_CommandPool = VK_NULL_HANDLE;
        }

        if (m_TransferTimelineSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_Device, m_TransferTimelineSemaphore, nullptr);
            m_TransferTimelineSemaphore = VK_NULL_HANDLE;
        }

        vkDestroyDevice(m_Device, nullptr);
        m_Device = VK_NULL_HANDLE;
    }

    if (m_Surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
        m_Surface = VK_NULL_HANDLE;
    }

    if (m_DebugMessenger != VK_NULL_HANDLE) {
        auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_Instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (func != nullptr) {
            func(m_Instance, m_DebugMessenger, nullptr);
        }
        m_DebugMessenger = VK_NULL_HANDLE;
    }

    if (m_Instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_Instance, nullptr);
        m_Instance = VK_NULL_HANDLE;
    }

        std::cout << "Vulkan context shut down" << std::endl;
}

VulkanMipmapCompute* VulkanContext::GetMipmapCompute() {
    if (!m_MipmapCompute) {
        m_MipmapCompute = std::make_unique<VulkanMipmapCompute>();
        m_MipmapCompute->Initialize(this);
    }
    return m_MipmapCompute.get();
}

void VulkanContext::CreateInstance() {
    if (m_EnableValidationLayers && !CheckValidationLayerSupport()) {
        throw std::runtime_error("Validation layers requested but not available");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Game Engine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Custom Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = GetRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<u32>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (m_EnableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<u32>(m_ValidationLayers.size());
        createInfo.ppEnabledLayerNames = m_ValidationLayers.data();

        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = DebugCallback;

        createInfo.pNext = &debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    if (vkCreateInstance(&createInfo, nullptr, &m_Instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance");
    }

    std::cout << "Vulkan instance created" << std::endl;
}

void VulkanContext::SetupDebugMessenger() {
    if (!m_EnableValidationLayers) {
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = DebugCallback;

    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(m_Instance, "vkCreateDebugUtilsMessengerEXT"));

    if (func == nullptr || func(m_Instance, &createInfo, nullptr, &m_DebugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("Failed to set up debug messenger");
    }

    std::cout << "Validation layers enabled" << std::endl;
}

void VulkanContext::CreateSurface(Window* window) {
    if (glfwCreateWindowSurface(m_Instance, window->GetNativeWindow(), nullptr, &m_Surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface");
    }

    std::cout << "Vulkan surface created" << std::endl;
}

void VulkanContext::PickPhysicalDevice() {
    u32 deviceCount = 0;
    vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

    for (const auto& device : devices) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);

        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            m_PhysicalDevice = device;
            std::cout << "Selected GPU: " << properties.deviceName << std::endl;
            break;
        }
    }

    if (m_PhysicalDevice == VK_NULL_HANDLE) {
        m_PhysicalDevice = devices[0];
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(m_PhysicalDevice, &properties);
        std::cout << "Selected GPU: " << properties.deviceName << std::endl;
    }
}

void VulkanContext::CreateLogicalDevice() {
    u32 queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, queueFamilies.data());

    u32 graphicsFamily = UINT32_MAX;
    u32 presentFamily = UINT32_MAX;

    for (u32 i = 0; i < queueFamilies.size(); i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsFamily = i;
        }

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_PhysicalDevice, i, m_Surface, &presentSupport);
        if (presentSupport) {
            presentFamily = i;
        }

        if (graphicsFamily != UINT32_MAX && presentFamily != UINT32_MAX) {
            break;
        }
    }

    if (graphicsFamily == UINT32_MAX || presentFamily == UINT32_MAX) {
        throw std::runtime_error("Failed to find suitable queue families");
    }

    m_GraphicsQueueFamily = graphicsFamily;
    m_PresentQueueFamily = presentFamily;

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<u32> uniqueQueueFamilies = {graphicsFamily, presentFamily};

    f32 queuePriority = 1.0f;
    for (u32 queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    // Enable timeline semaphore feature (Vulkan 1.2+)
    VkPhysicalDeviceTimelineSemaphoreFeatures timelineSemaphoreFeatures{};
    timelineSemaphoreFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
    timelineSemaphoreFeatures.timelineSemaphore = VK_TRUE;
    timelineSemaphoreFeatures.pNext = nullptr;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<u32>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.pNext = &timelineSemaphoreFeatures;

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    createInfo.enabledExtensionCount = static_cast<u32>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (m_EnableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<u32>(m_ValidationLayers.size());
        createInfo.ppEnabledLayerNames = m_ValidationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device");
    }

    vkGetDeviceQueue(m_Device, graphicsFamily, 0, &m_GraphicsQueue);
    vkGetDeviceQueue(m_Device, presentFamily, 0, &m_PresentQueue);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = m_GraphicsQueueFamily;

    if (vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics command pool");
    }

    // Create timeline semaphore for async transfers
    VkSemaphoreTypeCreateInfo timelineCreateInfo{};
    timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timelineCreateInfo.pNext = nullptr;
    timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timelineCreateInfo.initialValue = 0;

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.pNext = &timelineCreateInfo;
    semaphoreInfo.flags = 0;

    if (vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_TransferTimelineSemaphore) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create transfer timeline semaphore");
    }

    std::cout << "Logical device created" << std::endl;
    std::cout << "Graphics queue family: " << graphicsFamily << std::endl;
    std::cout << "Present queue family: " << presentFamily << std::endl;
}

void VulkanContext::CacheFormatCapabilities() {
    // Extended format set: PBR textures, HDR, compression, depth/stencil
    const std::vector<VkFormat> formatsToCache = {
        // 8-bit UNORM formats
        VK_FORMAT_R8_UNORM,
        VK_FORMAT_R8G8_UNORM,
        VK_FORMAT_R8G8B8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8A8_UNORM,

        // 8-bit SRGB formats
        VK_FORMAT_R8_SRGB,
        VK_FORMAT_R8G8_SRGB,
        VK_FORMAT_R8G8B8_SRGB,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_SRGB,

        // 16-bit float formats (HDR)
        VK_FORMAT_R16_SFLOAT,
        VK_FORMAT_R16G16_SFLOAT,
        VK_FORMAT_R16G16B16_SFLOAT,
        VK_FORMAT_R16G16B16A16_SFLOAT,

        // 16-bit UNORM formats
        VK_FORMAT_R16_UNORM,
        VK_FORMAT_R16G16_UNORM,
        VK_FORMAT_R16G16B16A16_UNORM,

        // 32-bit float formats (HDR)
        VK_FORMAT_R32_SFLOAT,
        VK_FORMAT_R32G32_SFLOAT,
        VK_FORMAT_R32G32B32_SFLOAT,
        VK_FORMAT_R32G32B32A32_SFLOAT,

        // Packed formats
        VK_FORMAT_A2B10G10R10_UNORM_PACK32,

        // Depth/Stencil formats
        VK_FORMAT_D16_UNORM,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,

        // BC compression formats (UNORM)
        VK_FORMAT_BC1_RGB_UNORM_BLOCK,
        VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
        VK_FORMAT_BC2_UNORM_BLOCK,
        VK_FORMAT_BC3_UNORM_BLOCK,
        VK_FORMAT_BC4_UNORM_BLOCK,
        VK_FORMAT_BC5_UNORM_BLOCK,
        VK_FORMAT_BC6H_SFLOAT_BLOCK,
        VK_FORMAT_BC7_UNORM_BLOCK,

        // BC compression formats (SRGB)
        VK_FORMAT_BC1_RGB_SRGB_BLOCK,
        VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
        VK_FORMAT_BC2_SRGB_BLOCK,
        VK_FORMAT_BC3_SRGB_BLOCK,
        VK_FORMAT_BC7_SRGB_BLOCK,
    };

    std::cout << "Caching format capabilities for " << formatsToCache.size() << " formats..." << std::endl;

    for (VkFormat format : formatsToCache) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, format, &properties);
        m_FormatCapabilities[format] = properties;
    }

    std::cout << "Format capabilities cached" << std::endl;
}

bool VulkanContext::CheckValidationLayerSupport() {
    u32 layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : m_ValidationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (std::strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

std::vector<const char*> VulkanContext::GetRequiredExtensions() {
    u32 glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (m_EnableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

const VkFormatProperties* VulkanContext::GetFormatProperties(VkFormat format) const {
    // Check cache first
    auto it = m_FormatCapabilities.find(format);
    if (it != m_FormatCapabilities.end()) {
        return &it->second;
    }

    // Not cached - query and cache on-demand
    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, format, &properties);
    m_FormatCapabilities[format] = properties;

    return &m_FormatCapabilities[format];
}

bool VulkanContext::SupportsLinearBlit(VkFormat format) const {
    const VkFormatProperties* props = GetFormatProperties(format);
    if (!props) {
        return false;
    }

    // Check if optimal tiling supports linear filtering for sampled images
    return (props->optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0;
}

bool VulkanContext::SupportsColorAttachment(VkFormat format) const {
    const VkFormatProperties* props = GetFormatProperties(format);
    if (!props) {
        return false;
    }

    return (props->optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) != 0;
}

bool VulkanContext::SupportsDepthStencilAttachment(VkFormat format) const {
    const VkFormatProperties* props = GetFormatProperties(format);
    if (!props) {
        return false;
    }

    return (props->optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
}

bool VulkanContext::SupportsTransferSrc(VkFormat format) const {
    const VkFormatProperties* props = GetFormatProperties(format);
    if (!props) {
        return false;
    }

    return (props->optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) != 0;
}

bool VulkanContext::SupportsTransferDst(VkFormat format) const {
    const VkFormatProperties* props = GetFormatProperties(format);
    if (!props) {
        return false;
    }

    return (props->optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) != 0;
}

bool VulkanContext::SupportsSampledImage(VkFormat format) const {
    const VkFormatProperties* props = GetFormatProperties(format);
    if (!props) {
        return false;
    }

    return (props->optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0;
}

bool VulkanContext::SupportsStorageImage(VkFormat format) const {
    const VkFormatProperties* props = GetFormatProperties(format);
    if (!props) {
        return false;
    }

    return (props->optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) != 0;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    (void)messageType;
    (void)pUserData;

    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "[Vulkan] " << pCallbackData->pMessage << std::endl;
    }

    return VK_FALSE;
}
