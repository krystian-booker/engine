#pragma once

#include "core/types.h"

#ifdef _DEBUG

// Forward declarations
class VulkanContext;
class Window;
class SceneManager;
class ECSCoordinator;
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
    void Init(VulkanContext* context, Window* window, VkRenderPass renderPass, SceneManager* sceneManager, ECSCoordinator* ecs);

    // Cleanup ImGui resources
    void Shutdown();

    // Begin a new ImGui frame
    void BeginFrame();

    // Render ImGui and record draw commands
    void Render(VkCommandBuffer commandBuffer);

private:
    // Helper methods for UI sections
    void RenderFileMenu();
    void RenderSceneMenu();
    void RenderHelpMenu();
    void RenderSceneHierarchyWindow();

    VulkanContext* m_Context = nullptr;
    Window* m_Window = nullptr;
    SceneManager* m_SceneManager = nullptr;
    ECSCoordinator* m_ECS = nullptr;
    VkDescriptorPool m_DescriptorPool = nullptr;

    // UI state
    bool m_ShowDemoWindow = false;
    bool m_ShowAboutWindow = false;
    bool m_ShowSceneHierarchyWindow = false;
};

#endif // _DEBUG
