#pragma once

#include "core/types.h"
#include <vulkan/vulkan.h>
#include <unordered_map>

#ifdef _DEBUG

// Forward declarations
class VulkanContext;
class Window;
class SceneManager;
class ECSCoordinator;
class ViewportManager;
class Viewport;

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

    // Setup dockspace and viewport windows (call after BeginFrame, before rendering offscreen targets)
    void SetupFrameLayout(ViewportManager* viewportManager);

    // Render ImGui and record draw commands
    void Render(VkCommandBuffer commandBuffer);

    // Render viewport windows (call during BeginFrame/Render cycle)
    void RenderViewportWindows(ViewportManager* viewportManager);

    // Get the focused viewport ID (0 = none)
    u32 GetFocusedViewportID() const { return m_FocusedViewportID; }

    // Check if project change was requested
    bool ShouldChangeProject() const { return m_ShouldChangeProject; }

private:
    // Helper methods for UI sections
    void RenderFileMenu();
    void RenderSceneMenu();
    void RenderWindowMenu();
    void RenderHelpMenu();
    void RenderSceneHierarchyWindow();
    void RenderInspectorWindow();
    void RenderConsoleWindow();
    void RenderViewportWindow(Viewport* viewport, const char* title);
    void SetupDockspace();

    VulkanContext* m_Context = nullptr;
    Window* m_Window = nullptr;
    SceneManager* m_SceneManager = nullptr;
    ECSCoordinator* m_ECS = nullptr;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

    // UI state
    bool m_ShowDemoWindow = false;
    bool m_ShowAboutWindow = false;
    bool m_ShowSceneHierarchyWindow = true;  // Show by default
    bool m_ShowInspectorWindow = true;       // Show by default
    bool m_ShowConsoleWindow = true;         // Show by default
    bool m_ShowChangeProjectDialog = false;  // Show change project confirmation

    // Viewport state
    u32 m_FocusedViewportID = 0;  // ID of currently focused viewport (0 = none)
    u32 m_FrameCount = 0;  // Frame counter to delay viewport texture display

    // Docking state
    bool m_DockspaceInitialized = false;  // Track if dockspace layout has been set up

    // Project change state
    bool m_ShouldChangeProject = false;  // Flag to signal project change request

    // Viewport texture descriptor management (separate from ImGui pool)
    VkDescriptorPool m_ViewportDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_ViewportDescriptorSetLayout = VK_NULL_HANDLE;
    std::unordered_map<u32, VkDescriptorSet> m_ViewportDescriptorSets;  // viewport ID -> descriptor set

    void CreateViewportDescriptorResources();
    void DestroyViewportDescriptorResources();
    VkDescriptorSet GetOrCreateViewportDescriptorSet(u32 viewportID, VkSampler sampler, VkImageView imageView);
};

#endif // _DEBUG
