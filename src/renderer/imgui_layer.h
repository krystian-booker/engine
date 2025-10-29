#pragma once

#include "core/types.h"

#ifdef _DEBUG

// Forward declarations
class VulkanContext;
class Window;
struct VkRenderPass_T;
typedef VkRenderPass_T* VkRenderPass;
struct VkDescriptorPool_T;
typedef VkDescriptorPool_T* VkDescriptorPool;
struct VkCommandBuffer_T;
typedef VkCommandBuffer_T* VkCommandBuffer;

// ImGuiLayer - Manages Dear ImGui integration
// Only available in debug builds
class ImGuiLayer
{
public:
    ImGuiLayer() = default;
    ~ImGuiLayer() = default;

    // Initialize ImGui with Vulkan and GLFW backends
    void Init(VulkanContext* context, Window* window, VkRenderPass renderPass);

    // Cleanup ImGui resources
    void Shutdown();

    // Begin a new ImGui frame
    void BeginFrame();

    // Render ImGui and record draw commands
    void Render(VkCommandBuffer commandBuffer);

private:
    VulkanContext* m_Context = nullptr;
    Window* m_Window = nullptr;
    VkDescriptorPool m_DescriptorPool = nullptr;
    bool m_ShowDemoWindow = false;  // Hidden by default, toggle via Help menu
    bool m_ShowAboutWindow = false;
};

#endif // _DEBUG
