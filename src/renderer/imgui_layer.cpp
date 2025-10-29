#include "imgui_layer.h"

#ifdef _DEBUG

#include "vulkan_context.h"
#include "platform/window.h"
#include "core/scene_manager.h"
#include "core/file_dialog.h"
#include "ecs/ecs_coordinator.h"
#include "ecs/components/transform.h"
#include "ecs/components/camera.h"
#include "ecs/components/renderable.h"
#include "ecs/components/rotator.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <string>
#include <functional>
#include <iostream>

void ImGuiLayer::Init(VulkanContext* context, Window* window, VkRenderPass renderPass, SceneManager* sceneManager, ECSCoordinator* ecs)
{
    m_Context = context;
    m_Window = window;
    m_SceneManager = sceneManager;
    m_ECS = ecs;

    // Create descriptor pool for ImGui
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000;
    poolInfo.poolSizeCount = static_cast<u32>(sizeof(poolSizes) / sizeof(VkDescriptorPoolSize));
    poolInfo.pPoolSizes = poolSizes;

    if (vkCreateDescriptorPool(context->GetDevice(), &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create ImGui descriptor pool!");
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable keyboard controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Enable docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;    // Enable multi-viewport

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // When viewports are enabled, tweak WindowRounding/WindowBg so platform windows can look identical to regular ones
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(window->GetNativeWindow(), true);

    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.ApiVersion = VK_API_VERSION_1_2;
    initInfo.Instance = context->GetInstance();
    initInfo.PhysicalDevice = context->GetPhysicalDevice();
    initInfo.Device = context->GetDevice();
    initInfo.QueueFamily = context->GetGraphicsQueueFamily();
    initInfo.Queue = context->GetGraphicsQueue();
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = m_DescriptorPool;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = 2;
    initInfo.Allocator = nullptr;
    initInfo.CheckVkResultFn = nullptr;

    // Set pipeline info for main viewport (new API as of 2025/09/26)
    initInfo.PipelineInfoMain.RenderPass = renderPass;
    initInfo.PipelineInfoMain.Subpass = 0;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&initInfo);

    // Create the main pipeline after init
    ImGui_ImplVulkan_CreateMainPipeline(&initInfo.PipelineInfoMain);
}

void ImGuiLayer::Shutdown()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_DescriptorPool != nullptr)
    {
        vkDestroyDescriptorPool(m_Context->GetDevice(), m_DescriptorPool, nullptr);
        m_DescriptorPool = nullptr;
    }
}

void ImGuiLayer::BeginFrame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::Render(VkCommandBuffer commandBuffer)
{
    // Main menu bar
    if (ImGui::BeginMainMenuBar())
    {
        RenderFileMenu();
        RenderSceneMenu();
        RenderHelpMenu();
        ImGui::EndMainMenuBar();
    }

    // Show demo window
    if (m_ShowDemoWindow)
    {
        ImGui::ShowDemoWindow(&m_ShowDemoWindow);
    }

    // Show about window
    if (m_ShowAboutWindow)
    {
        ImGui::Begin("About", &m_ShowAboutWindow);
        ImGui::Text("Game Engine");
        ImGui::Separator();
        ImGui::Text("A custom game engine with Vulkan renderer");
        ImGui::Text("Built with Dear ImGui v%s", IMGUI_VERSION);
        ImGui::Separator();
        if (ImGui::Button("Close"))
        {
            m_ShowAboutWindow = false;
        }
        ImGui::End();
    }

    // Show scene hierarchy window
    if (m_ShowSceneHierarchyWindow)
    {
        RenderSceneHierarchyWindow();
    }

    // Render ImGui
    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();
    ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);

    // Update and render additional platform windows (if enabled)
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void ImGuiLayer::RenderFileMenu()
{
    if (ImGui::BeginMenu("File"))
    {
        // New Scene
        if (ImGui::MenuItem("New Scene", "Ctrl+N"))
        {
            // TODO: Prompt to save if dirty
            if (m_SceneManager)
            {
                m_SceneManager->NewScene();
            }
        }

        // Open Scene
        if (ImGui::MenuItem("Open Scene...", "Ctrl+O"))
        {
            // TODO: Prompt to save if dirty
            if (m_SceneManager)
            {
                auto filepath = FileDialog::OpenFile(
                    "Open Scene",
                    "assets/scenes",
                    {"Scene Files", "*.scene"}
                );

                if (filepath.has_value())
                {
                    m_SceneManager->LoadScene(filepath.value());
                }
            }
        }

        ImGui::Separator();

        // Save Scene
        if (ImGui::MenuItem("Save Scene", "Ctrl+S", false, m_SceneManager && m_SceneManager->HasCurrentFile()))
        {
            if (m_SceneManager)
            {
                m_SceneManager->SaveScene();
            }
        }

        // Save Scene As
        if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S"))
        {
            if (m_SceneManager)
            {
                auto filepath = FileDialog::SaveFile(
                    "Save Scene As",
                    "assets/scenes/untitled.scene",
                    {"Scene Files", "*.scene"}
                );

                if (filepath.has_value())
                {
                    m_SceneManager->SaveSceneAs(filepath.value());
                }
            }
        }

        ImGui::Separator();

        // Recent Scenes
        if (m_SceneManager && ImGui::BeginMenu("Recent Scenes"))
        {
            const auto& recentScenes = m_SceneManager->GetRecentScenes();

            if (recentScenes.empty())
            {
                ImGui::MenuItem("(No recent scenes)", nullptr, false, false);
            }
            else
            {
                for (const auto& scenePath : recentScenes)
                {
                    if (ImGui::MenuItem(scenePath.c_str()))
                    {
                        m_SceneManager->LoadScene(scenePath);
                    }
                }
            }

            ImGui::EndMenu();
        }

        ImGui::Separator();

        // Exit
        if (ImGui::MenuItem("Exit", "Alt+F4"))
        {
            // TODO: Prompt to save if dirty
            glfwSetWindowShouldClose(m_Window->GetNativeWindow(), GLFW_TRUE);
        }

        ImGui::EndMenu();
    }
}

void ImGuiLayer::RenderSceneMenu()
{
    if (ImGui::BeginMenu("Scene"))
    {
        if (ImGui::MenuItem("Scene Hierarchy", nullptr, m_ShowSceneHierarchyWindow))
        {
            m_ShowSceneHierarchyWindow = !m_ShowSceneHierarchyWindow;
        }

        ImGui::EndMenu();
    }
}

void ImGuiLayer::RenderHelpMenu()
{
    if (ImGui::BeginMenu("Help"))
    {
        if (ImGui::MenuItem("About"))
        {
            m_ShowAboutWindow = true;
        }
        if (ImGui::MenuItem("Toggle Demo Window"))
        {
            m_ShowDemoWindow = !m_ShowDemoWindow;
        }
        ImGui::EndMenu();
    }
}

void ImGuiLayer::RenderSceneHierarchyWindow()
{
    if (!ImGui::Begin("Scene Hierarchy", &m_ShowSceneHierarchyWindow))
    {
        ImGui::End();
        return;
    }

    if (!m_ECS)
    {
        ImGui::Text("No ECS coordinator available");
        ImGui::End();
        return;
    }

    // Get all entities with Transform component
    auto transforms = m_ECS->GetComponentRegistry()->GetComponentArray<Transform>();

    if (transforms->Size() == 0)
    {
        ImGui::Text("(Empty scene)");
        ImGui::End();
        return;
    }

    // Display entities in tree structure
    for (size_t i = 0; i < transforms->Size(); ++i)
    {
        Entity entity = transforms->GetEntity(i);

        // Only show root entities (entities without parent)
        Entity parent = m_ECS->GetParent(entity);
        if (parent.IsValid())
        {
            continue;
        }

        // Recursive function to display entity tree
        std::function<void(Entity)> displayEntityTree = [&](Entity e) {
            // Build entity label
            std::string label = "Entity " + std::to_string(e.index) + ":" + std::to_string(e.generation);

            // Check if entity has children
            const auto& children = m_ECS->GetChildren(e);
            bool hasChildren = !children.empty();

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
            if (!hasChildren)
            {
                flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            }

            bool nodeOpen = ImGui::TreeNodeEx(label.c_str(), flags);

            // Display components inline
            ImGui::SameLine();
            ImGui::TextDisabled("[");
            bool firstComponent = true;

            if (m_ECS->HasComponent<Transform>(e))
            {
                if (!firstComponent) {
                    ImGui::SameLine();
                    ImGui::TextDisabled(",");
                }
                ImGui::SameLine();
                ImGui::TextDisabled("T");
                firstComponent = false;
            }
            if (m_ECS->HasComponent<Camera>(e))
            {
                if (!firstComponent) {
                    ImGui::SameLine();
                    ImGui::TextDisabled(",");
                }
                ImGui::SameLine();
                ImGui::TextDisabled("C");
                firstComponent = false;
            }
            if (m_ECS->HasComponent<Renderable>(e))
            {
                if (!firstComponent) {
                    ImGui::SameLine();
                    ImGui::TextDisabled(",");
                }
                ImGui::SameLine();
                ImGui::TextDisabled("R");
                firstComponent = false;
            }
            if (m_ECS->HasComponent<Rotator>(e))
            {
                if (!firstComponent) {
                    ImGui::SameLine();
                    ImGui::TextDisabled(",");
                }
                ImGui::SameLine();
                ImGui::TextDisabled("Rot");
                firstComponent = false;
            }

            ImGui::SameLine(); ImGui::TextDisabled("]");

            if (nodeOpen && hasChildren)
            {
                for (Entity child : children)
                {
                    displayEntityTree(child);
                }
                ImGui::TreePop();
            }
        };

        displayEntityTree(entity);
    }

    ImGui::End();
}

#endif // _DEBUG
