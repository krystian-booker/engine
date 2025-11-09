#include "imgui_layer.h"

#ifdef _DEBUG

#include "vulkan_context.h"
#include "platform/window.h"
#include "platform/input.h"
#include "core/scene_manager.h"
#include "core/file_dialog.h"
#include "core/engine_settings.h"
#include "renderer/viewport.h"
#include "renderer/viewport_manager.h"
#include "ecs/ecs_coordinator.h"
#include "ecs/components/transform.h"
#include "ecs/components/camera.h"
#include "ecs/components/renderable.h"
#include "ecs/components/rotator.h"
#include "ecs/components/light.h"
#include "ecs/components/name.h"
#include "editor/editor_state.h"
#include "editor/entity_inspector.h"
#include "editor/entity_picker.h"
#include <imgui.h>
#include <imgui_internal.h>  // For DockBuilder API
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include "../external/ImGuizmo/ImGuizmo.h"
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
    // Try without FREE_DESCRIPTOR_SET_BIT to see if that's the issue
    poolInfo.flags = 0;
    poolInfo.maxSets = 2000;  // Increased from 1000
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
    // Pass false to NOT install GLFW callbacks - we'll chain them manually to preserve Input system
    ImGui_ImplGlfw_InitForVulkan(window->GetNativeWindow(), false);

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

    // Create viewport descriptor resources (separate from ImGui pool)
    CreateViewportDescriptorResources();

    // Initialize editor systems
    m_EditorState = new EditorState();
    m_EntityInspector = new EntityInspector(ecs);
    m_EntityPicker = new EntityPicker(ecs);
}

void ImGuiLayer::Shutdown()
{
    // Clean up editor systems
    delete m_EntityPicker;
    m_EntityPicker = nullptr;

    delete m_EntityInspector;
    m_EntityInspector = nullptr;

    delete m_EditorState;
    m_EditorState = nullptr;

    // Destroy viewport descriptor resources first
    DestroyViewportDescriptorResources();

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_DescriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(m_Context->GetDevice(), m_DescriptorPool, nullptr);
        m_DescriptorPool = VK_NULL_HANDLE;
    }
}

void ImGuiLayer::BeginFrame()
{
    m_FrameCount++;
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Handle keyboard shortcuts
    HandleKeyboardShortcuts();
}

void ImGuiLayer::SetupFrameLayout(ViewportManager* viewportManager)
{
    // Setup dockspace to fill entire window
    SetupDockspace();

    // Render viewport windows to establish their sizes BEFORE offscreen rendering
    if (viewportManager)
    {
        RenderViewportWindows(viewportManager);
    }
}

void ImGuiLayer::Render(VkCommandBuffer commandBuffer)
{
    // Main menu bar
    if (ImGui::BeginMainMenuBar())
    {
        RenderFileMenu();
        RenderSceneMenu();
        RenderWindowMenu();
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

    // Show editor panels
    if (m_ShowSceneHierarchyWindow)
    {
        RenderSceneHierarchyWindow();
    }

    if (m_ShowInspectorWindow)
    {
        RenderInspectorWindow();
    }

    if (m_ShowConsoleWindow)
    {
        RenderConsoleWindow();
    }

    // Show change project confirmation dialog
    if (m_ShowChangeProjectDialog)
    {
        ImGui::OpenPopup("Change Project");
        m_ShowChangeProjectDialog = false;  // Only open once
    }

    // Change Project confirmation popup
    if (ImGui::BeginPopupModal("Change Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("This will close the current project and show the project picker.");
        ImGui::Text("Any unsaved changes will be lost.");
        ImGui::Separator();

        if (ImGui::Button("OK", ImVec2(120, 0)))
        {
            // Clear the default project setting so picker shows on restart
            EngineSettings settings = EngineSettings::Load();
            settings.skipProjectPicker = false;
            settings.defaultProjectPath = "";
            settings.Save();

            // Set flag to signal project change
            m_ShouldChangeProject = true;

            ImGui::CloseCurrentPopup();
            // Close the application (main loop will detect the flag and restart)
            glfwSetWindowShouldClose(m_Window->GetNativeWindow(), GLFW_TRUE);
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
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

        // Change Project
        if (ImGui::MenuItem("Change Project..."))
        {
            // TODO: Prompt to save if dirty
            m_ShowChangeProjectDialog = true;
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
        if (ImGui::MenuItem("Create Empty Entity"))
        {
            if (m_ECS)
            {
                m_ECS->CreateEntity();
            }
        }

        ImGui::EndMenu();
    }
}

void ImGuiLayer::RenderWindowMenu()
{
    if (ImGui::BeginMenu("Window"))
    {
        if (ImGui::MenuItem("Scene Hierarchy", nullptr, m_ShowSceneHierarchyWindow))
        {
            m_ShowSceneHierarchyWindow = !m_ShowSceneHierarchyWindow;
        }

        if (ImGui::MenuItem("Inspector", nullptr, m_ShowInspectorWindow))
        {
            m_ShowInspectorWindow = !m_ShowInspectorWindow;
        }

        if (ImGui::MenuItem("Console", nullptr, m_ShowConsoleWindow))
        {
            m_ShowConsoleWindow = !m_ShowConsoleWindow;
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

    if (!m_ECS || !m_EditorState)
    {
        ImGui::Text("No ECS coordinator available");
        ImGui::End();
        return;
    }

    // Toolbar for creating entities
    if (ImGui::Button("+ Entity"))
    {
        CreateEntity("Entity");
    }

    ImGui::SameLine();
    if (ImGui::BeginMenu("+ Preset"))
    {
        if (ImGui::MenuItem("Cube"))
        {
            CreateEntity("Cube");
            // TODO: Add cube mesh
        }
        if (ImGui::MenuItem("Point Light"))
        {
            Entity light = m_ECS->CreateEntity();
            m_ECS->AddComponent(light, Transform{});
            m_ECS->AddComponent(light, Light{.type = LightType::Point});
            m_ECS->AddComponent(light, Name("Point Light"));
            m_EditorState->SetSelectedEntity(light);
        }
        if (ImGui::MenuItem("Directional Light"))
        {
            Entity light = m_ECS->CreateEntity();
            m_ECS->AddComponent(light, Transform{});
            m_ECS->AddComponent(light, Light{.type = LightType::Directional});
            m_ECS->AddComponent(light, Name("Directional Light"));
            m_EditorState->SetSelectedEntity(light);
        }
        if (ImGui::MenuItem("Camera"))
        {
            Entity camera = m_ECS->CreateEntity();
            m_ECS->AddComponent(camera, Transform{});
            m_ECS->AddComponent(camera, Camera{});
            m_ECS->AddComponent(camera, Name("Camera"));
            m_EditorState->SetSelectedEntity(camera);
        }

        ImGui::EndMenu();
    }

    ImGui::Separator();

    // Get all entities with Transform component
    auto transforms = m_ECS->GetComponentRegistry()->GetComponentArray<Transform>();

    if (transforms->Size() == 0)
    {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(Empty scene)");
        ImGui::Text("Click '+ Entity' to create one");
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

        // Render entity tree
        RenderEntityTree(entity);
    }

    // Handle deferred entity deletions
    for (Entity entity : m_EntitiesToDelete)
    {
        DeleteEntity(entity);
    }
    m_EntitiesToDelete.clear();

    ImGui::End();
}

void ImGuiLayer::RenderViewportWindows(ViewportManager* viewportManager)
{
    if (!viewportManager)
    {
        return;
    }

    auto viewports = viewportManager->GetAllViewports();
    for (Viewport* viewport : viewports)
    {
        if (!viewport || !viewport->IsValid())
        {
            continue;
        }

        // Determine viewport title based on type
        const char* title = (viewport->GetType() == ViewportType::Scene) ? "Scene" : "Game";
        RenderViewportWindow(viewport, title);
    }
}

void ImGuiLayer::RenderViewportWindow(Viewport* viewport, const char* title)
{
    if (!viewport || !viewport->IsValid())
    {
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    bool isOpen = true;
    if (ImGui::Begin(title, &isOpen, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        // Track focused viewport
        if (ImGui::IsWindowFocused())
        {
            m_FocusedViewportID = viewport->GetID();
            if (m_EditorState)
            {
                m_EditorState->SetFocusedViewportID(viewport->GetID());
            }
        }

        // Handle viewport input (entity picking)
        HandleViewportInput(viewport);

        // Get available content region
        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();

        // Resize viewport if window size changed
        if (viewportPanelSize.x > 0 && viewportPanelSize.y > 0)
        {
            u32 newWidth = static_cast<u32>(viewportPanelSize.x);
            u32 newHeight = static_cast<u32>(viewportPanelSize.y);

            if (newWidth != viewport->GetWidth() || newHeight != viewport->GetHeight())
            {
                viewport->Resize(newWidth, newHeight);
            }
        }

        // Only display viewport texture if it has been rendered at least once
        // This avoids sampling from images that are still in UNDEFINED layout
        // Note: HasBeenRendered() is set after viewport command buffer is submitted,
        // so the image should be in the correct layout by the time ImGui samples it
        // ALSO: Wait a few frames before trying to display textures to let ImGui fully initialize
        if (viewport->HasBeenRendered() && m_FrameCount > 3)
        {
            VulkanRenderTarget& renderTarget = viewport->GetRenderTarget();
            VkImageView colorView = renderTarget.GetColorImageView();
            VkSampler sampler = renderTarget.GetColorSampler();

            if (colorView != VK_NULL_HANDLE && sampler != VK_NULL_HANDLE)
            {
                // Use custom descriptor set management (bypasses ImGui's potentially exhausted pool)
                VkDescriptorSet descriptorSet = GetOrCreateViewportDescriptorSet(viewport->GetID(), sampler, colorView);

                if (descriptorSet != VK_NULL_HANDLE)
                {
                    ImGui::Image(descriptorSet, viewportPanelSize);

                    // Render gizmo on top of viewport
                    RenderGizmo(viewport);
                }
                else
                {
                    ImGui::Text("Failed to create descriptor set for viewport texture");
                    ImGui::Text("(Descriptor pool may be exhausted)");
                }
            }
            else
            {
                ImGui::Text("Viewport texture not available");
            }
        }
        else
        {
            // Viewport not yet rendered - show placeholder
            ImGui::Text("Viewport rendering...");
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();

    if (!isOpen)
    {
        // User closed viewport window - could handle this if needed
    }
}

void ImGuiLayer::CreateViewportDescriptorResources()
{
    VkDevice device = m_Context->GetDevice();

    // Create descriptor set layout for viewport textures
    // Must match ImGui's expected layout for ImGui::Image() to work
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_ViewportDescriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create viewport descriptor set layout!");
    }

    // Create descriptor pool for viewport textures
    // Allocate enough for reasonable number of viewports (e.g., 10 viewports)
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 10;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 10;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_ViewportDescriptorPool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create viewport descriptor pool!");
    }
}

void ImGuiLayer::DestroyViewportDescriptorResources()
{
    VkDevice device = m_Context->GetDevice();

    // Free all descriptor sets (required before destroying pool if using FREE_DESCRIPTOR_SET_BIT)
    for (auto& pair : m_ViewportDescriptorSets)
    {
        if (pair.second != VK_NULL_HANDLE)
        {
            vkFreeDescriptorSets(device, m_ViewportDescriptorPool, 1, &pair.second);
        }
    }
    m_ViewportDescriptorSets.clear();

    // Destroy descriptor pool
    if (m_ViewportDescriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(device, m_ViewportDescriptorPool, nullptr);
        m_ViewportDescriptorPool = VK_NULL_HANDLE;
    }

    // Destroy descriptor set layout
    if (m_ViewportDescriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device, m_ViewportDescriptorSetLayout, nullptr);
        m_ViewportDescriptorSetLayout = VK_NULL_HANDLE;
    }
}

VkDescriptorSet ImGuiLayer::GetOrCreateViewportDescriptorSet(u32 viewportID, VkSampler sampler, VkImageView imageView)
{
    VkDevice device = m_Context->GetDevice();

    // Check if descriptor set already exists for this viewport
    auto it = m_ViewportDescriptorSets.find(viewportID);
    if (it != m_ViewportDescriptorSets.end() && it->second != VK_NULL_HANDLE)
    {
        // Descriptor set exists - update it with new sampler/imageView
        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = sampler;
        imageInfo.imageView = imageView;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet writeDesc{};
        writeDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDesc.dstSet = it->second;
        writeDesc.dstBinding = 0;
        writeDesc.dstArrayElement = 0;
        writeDesc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeDesc.descriptorCount = 1;
        writeDesc.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(device, 1, &writeDesc, 0, nullptr);
        return it->second;
    }

    // Descriptor set doesn't exist - allocate a new one
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_ViewportDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_ViewportDescriptorSetLayout;

    VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);
    if (result != VK_SUCCESS)
    {
        // Allocation failed - return null handle
        std::cerr << "Failed to allocate viewport descriptor set: " << result << std::endl;
        return VK_NULL_HANDLE;
    }

    // Update the newly allocated descriptor set
    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = sampler;
    imageInfo.imageView = imageView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet writeDesc{};
    writeDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDesc.dstSet = descriptorSet;
    writeDesc.dstBinding = 0;
    writeDesc.dstArrayElement = 0;
    writeDesc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeDesc.descriptorCount = 1;
    writeDesc.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, 1, &writeDesc, 0, nullptr);

    // Cache the descriptor set
    m_ViewportDescriptorSets[viewportID] = descriptorSet;

    return descriptorSet;
}

void ImGuiLayer::SetupDockspace()
{
    // Create a dockspace that fills the entire viewport
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
    windowFlags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("DockSpace", nullptr, windowFlags);
    ImGui::PopStyleVar(3);

    // Create the dockspace
    ImGuiID dockspaceID = ImGui::GetID("MainDockspace");
    ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    // Set up default layout on first run
    if (!m_DockspaceInitialized)
    {
        m_DockspaceInitialized = true;

        // Clear any existing layout
        ImGui::DockBuilderRemoveNode(dockspaceID);
        ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceID, viewport->WorkSize);

        // Split the dockspace into regions
        ImGuiID dockLeft, dockRight, dockBottom;
        ImGuiID dockMain = dockspaceID;

        // Split off left (20% for Scene Hierarchy)
        dockLeft = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.2f, nullptr, &dockMain);

        // Split off right (25% for Inspector)
        dockRight = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.25f, nullptr, &dockMain);

        // Split off bottom (25% for Console)
        dockBottom = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.25f, nullptr, &dockMain);

        // Main area (dockMain) will contain both Scene and Game viewports as tabs
        // No need to split - just dock both windows to the same node

        // Dock windows to their default positions
        ImGui::DockBuilderDockWindow("Scene Hierarchy", dockLeft);
        ImGui::DockBuilderDockWindow("Inspector", dockRight);
        ImGui::DockBuilderDockWindow("Console", dockBottom);
        ImGui::DockBuilderDockWindow("Game", dockMain);   // Dock Game first
        ImGui::DockBuilderDockWindow("Scene", dockMain);  // Dock Scene second (becomes active tab)

        ImGui::DockBuilderFinish(dockspaceID);
    }

    ImGui::End();
}

void ImGuiLayer::RenderInspectorWindow()
{
    if (!ImGui::Begin("Inspector", &m_ShowInspectorWindow))
    {
        ImGui::End();
        return;
    }

    if (!m_ECS || !m_EntityInspector || !m_EditorState)
    {
        ImGui::Text("No ECS coordinator available");
        ImGui::End();
        return;
    }

    // Render inspector for selected entity
    Entity selected = m_EditorState->GetSelectedEntity();
    m_EntityInspector->Render(selected);

    ImGui::End();
}

void ImGuiLayer::RenderConsoleWindow()
{
    if (!ImGui::Begin("Console", &m_ShowConsoleWindow))
    {
        ImGui::End();
        return;
    }

    // Header with buttons
    if (ImGui::Button("Clear"))
    {
        // TODO: Clear console messages
    }
    ImGui::SameLine();

    static bool showInfo = true;
    static bool showWarnings = true;
    static bool showErrors = true;

    ImGui::Checkbox("Info", &showInfo);
    ImGui::SameLine();
    ImGui::Checkbox("Warnings", &showWarnings);
    ImGui::SameLine();
    ImGui::Checkbox("Errors", &showErrors);

    ImGui::Separator();

    // Console output area
    ImGui::BeginChild("ConsoleOutput", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

    // TODO: Display actual log messages
    // For now, show placeholder messages
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "[Info] Console initialized");
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "[Warning] This is a placeholder console");
    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[Error] Example error message");

    ImGui::EndChild();

    ImGui::End();
}

void ImGuiLayer::RenderEntityTree(Entity entity)
{
    // Build entity label - use Name component if available
    std::string label;
    if (m_ECS->HasComponent<Name>(entity))
    {
        const Name& name = m_ECS->GetComponent<Name>(entity);
        label = std::string(name.GetName()) + " (" + std::to_string(entity.index) + ":" + std::to_string(entity.generation) + ")";
    }
    else
    {
        label = "Entity " + std::to_string(entity.index) + ":" + std::to_string(entity.generation);
    }

    // Check if entity has children
    const auto& children = m_ECS->GetChildren(entity);
    bool hasChildren = !children.empty();

    // Build tree node flags
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;

    // Highlight selected entity
    if (m_EditorState->GetSelectedEntity() == entity)
    {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    if (!hasChildren)
    {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)entity.index, flags, "%s", label.c_str());

    // Handle selection on click
    if (ImGui::IsItemClicked())
    {
        m_EditorState->SetSelectedEntity(entity);
    }

    // Context menu
    if (ImGui::BeginPopupContextItem())
    {
        RenderEntityContextMenu(entity);
        ImGui::EndPopup();
    }

    // Display components inline
    ImGui::SameLine();
    ImGui::TextDisabled("[");
    bool firstComponent = true;

    if (m_ECS->HasComponent<Camera>(entity))
    {
        if (!firstComponent) { ImGui::SameLine(); ImGui::TextDisabled(","); }
        ImGui::SameLine(); ImGui::TextDisabled("C");
        firstComponent = false;
    }
    if (m_ECS->HasComponent<Light>(entity))
    {
        if (!firstComponent) { ImGui::SameLine(); ImGui::TextDisabled(","); }
        ImGui::SameLine(); ImGui::TextDisabled("L");
        firstComponent = false;
    }
    if (m_ECS->HasComponent<Renderable>(entity))
    {
        if (!firstComponent) { ImGui::SameLine(); ImGui::TextDisabled(","); }
        ImGui::SameLine(); ImGui::TextDisabled("R");
        firstComponent = false;
    }
    if (m_ECS->HasComponent<Rotator>(entity))
    {
        if (!firstComponent) { ImGui::SameLine(); ImGui::TextDisabled(","); }
        ImGui::SameLine(); ImGui::TextDisabled("Rot");
        firstComponent = false;
    }

    ImGui::SameLine(); ImGui::TextDisabled("]");

    // Render children if node is open
    if (nodeOpen && hasChildren)
    {
        for (Entity child : children)
        {
            RenderEntityTree(child);
        }
        ImGui::TreePop();
    }
}

void ImGuiLayer::RenderEntityContextMenu(Entity entity)
{
    if (ImGui::MenuItem("Delete"))
    {
        // Defer deletion to avoid modifying ECS during iteration
        m_EntitiesToDelete.push_back(entity);

        // Clear selection if deleting selected entity
        if (m_EditorState->GetSelectedEntity() == entity)
        {
            m_EditorState->ClearSelection();
        }
    }

    ImGui::Separator();

    if (ImGui::BeginMenu("Add Component"))
    {
        if (!m_ECS->HasComponent<Light>(entity) && ImGui::MenuItem("Light"))
        {
            m_ECS->AddComponent(entity, Light{});
        }
        if (!m_ECS->HasComponent<Camera>(entity) && ImGui::MenuItem("Camera"))
        {
            m_ECS->AddComponent(entity, Camera{});
        }
        if (!m_ECS->HasComponent<Renderable>(entity) && ImGui::MenuItem("Renderable"))
        {
            m_ECS->AddComponent(entity, Renderable{});
        }
        if (!m_ECS->HasComponent<Rotator>(entity) && ImGui::MenuItem("Rotator"))
        {
            m_ECS->AddComponent(entity, Rotator{});
        }
        ImGui::EndMenu();
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Duplicate"))
    {
        DuplicateEntity(entity);
    }
}

void ImGuiLayer::CreateEntity(const char* name)
{
    Entity entity = m_ECS->CreateEntity();
    m_ECS->AddComponent(entity, Transform{});
    m_ECS->AddComponent(entity, Name(name));
    m_EditorState->SetSelectedEntity(entity);
}

void ImGuiLayer::DeleteEntity(Entity entity)
{
    if (!entity.IsValid())
    {
        return;
    }

    // Recursively delete children first
    const auto& children = m_ECS->GetChildren(entity);
    for (Entity child : children)
    {
        DeleteEntity(child);
    }

    m_ECS->DestroyEntity(entity);
}

void ImGuiLayer::DuplicateEntity(Entity source)
{
    if (!source.IsValid() || !m_ECS->HasComponent<Transform>(source))
    {
        return;
    }

    Entity duplicate = m_ECS->CreateEntity();

    // Copy Transform
    Transform sourceTransform = m_ECS->GetComponent<Transform>(source);
    sourceTransform.localPosition.x += 1.0f;  // Offset slightly
    m_ECS->AddComponent(duplicate, sourceTransform);

    // Copy Name (with " Copy" suffix)
    if (m_ECS->HasComponent<Name>(source))
    {
        Name sourceName = m_ECS->GetComponent<Name>(source);
        std::string newName = std::string(sourceName.GetName()) + " Copy";
        m_ECS->AddComponent(duplicate, Name(newName.c_str()));
    }
    else
    {
        m_ECS->AddComponent(duplicate, Name("Entity Copy"));
    }

    // Copy other components
    if (m_ECS->HasComponent<Renderable>(source))
    {
        m_ECS->AddComponent(duplicate, m_ECS->GetComponent<Renderable>(source));
    }
    if (m_ECS->HasComponent<Light>(source))
    {
        m_ECS->AddComponent(duplicate, m_ECS->GetComponent<Light>(source));
    }
    if (m_ECS->HasComponent<Camera>(source))
    {
        Camera cam = m_ECS->GetComponent<Camera>(source);
        cam.isEditorCamera = false;  // Don't duplicate as editor camera
        m_ECS->AddComponent(duplicate, cam);
    }
    if (m_ECS->HasComponent<Rotator>(source))
    {
        m_ECS->AddComponent(duplicate, m_ECS->GetComponent<Rotator>(source));
    }

    m_EditorState->SetSelectedEntity(duplicate);
}

void ImGuiLayer::HandleKeyboardShortcuts()
{
    // Don't handle shortcuts if ImGui is capturing keyboard
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard && !io.WantTextInput)
    {
        // Allow shortcuts even when keyboard is captured, unless typing in a text field
    }
    else if (io.WantTextInput)
    {
        // Don't process shortcuts when typing
        return;
    }

    // Gizmo mode shortcuts (W/E/R like Unity)
    if (Input::IsKeyPressed(KeyCode::W))
    {
        m_EditorState->SetGizmoMode(EditorState::GizmoMode::Translate);
    }
    else if (Input::IsKeyPressed(KeyCode::E))
    {
        m_EditorState->SetGizmoMode(EditorState::GizmoMode::Rotate);
    }
    else if (Input::IsKeyPressed(KeyCode::R))
    {
        m_EditorState->SetGizmoMode(EditorState::GizmoMode::Scale);
    }

    // Delete selected entity
    if (Input::IsKeyPressed(KeyCode::Delete) || Input::IsKeyPressed(KeyCode::Backspace))
    {
        Entity selected = m_EditorState->GetSelectedEntity();
        if (selected.IsValid())
        {
            DeleteEntity(selected);
            m_EditorState->ClearSelection();
        }
    }
}

void ImGuiLayer::HandleViewportInput(Viewport* viewport)
{
    if (!viewport || !m_EntityPicker || !m_EditorState)
    {
        return;
    }

    // Only handle input if viewport window is hovered (not just focused)
    if (!ImGui::IsWindowHovered())
    {
        return;
    }

    // Handle mouse click for entity selection
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemActive())
    {
        // Get mouse position relative to viewport
        ImVec2 mousePos = ImGui::GetMousePos();
        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
        ImVec2 contentMax = ImGui::GetWindowContentRegionMax();

        Vec2 viewportPos(
            mousePos.x - windowPos.x - contentMin.x,
            mousePos.y - windowPos.y - contentMin.y
        );

        Vec2 viewportSize(
            contentMax.x - contentMin.x,
            contentMax.y - contentMin.y
        );

        // Make sure click is within viewport bounds
        if (viewportPos.x >= 0 && viewportPos.y >= 0 &&
            viewportPos.x < viewportSize.x && viewportPos.y < viewportSize.y)
        {
            // Get viewport camera
            Entity cameraEntity = viewport->GetCamera();
            if (cameraEntity.IsValid() && m_ECS->HasComponent<Camera>(cameraEntity))
            {
                Camera& camera = m_ECS->GetComponent<Camera>(cameraEntity);

                // Pick entity
                Entity picked = m_EntityPicker->PickEntity(
                    viewportPos, viewportSize,
                    camera.viewMatrix, camera.projectionMatrix
                );

                if (picked.IsValid())
                {
                    m_EditorState->SetSelectedEntity(picked);
                }
                else
                {
                    // Clicked on empty space - deselect
                    m_EditorState->ClearSelection();
                }
            }
        }
    }
}

void ImGuiLayer::RenderGizmo(Viewport* viewport)
{
    if (!viewport || !m_EditorState || !m_ECS)
    {
        return;
    }

    // Only render gizmo if an entity is selected
    Entity selected = m_EditorState->GetSelectedEntity();
    if (!selected.IsValid() || !m_ECS->HasComponent<Transform>(selected))
    {
        return;
    }

    // Get viewport camera
    Entity cameraEntity = viewport->GetCamera();
    if (!cameraEntity.IsValid() || !m_ECS->HasComponent<Camera>(cameraEntity))
    {
        return;
    }

    Camera& camera = m_ECS->GetComponent<Camera>(cameraEntity);
    Transform& entityTransform = m_ECS->GetMutableComponent<Transform>(selected);

    // Setup ImGuizmo
    ImGuizmo::SetOrthographic(camera.projection == CameraProjection::Orthographic);
    ImGuizmo::SetDrawlist();

    // Set viewport rect (where gizmo should render)
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 windowSize = ImGui::GetWindowSize();
    ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
    ImVec2 contentMax = ImGui::GetWindowContentRegionMax();

    ImGuizmo::SetRect(
        windowPos.x + contentMin.x,
        windowPos.y + contentMin.y,
        contentMax.x - contentMin.x,
        contentMax.y - contentMin.y
    );

    // Convert gizmo mode
    ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
    switch (m_EditorState->GetGizmoMode())
    {
        case EditorState::GizmoMode::Translate:
            operation = ImGuizmo::TRANSLATE;
            break;
        case EditorState::GizmoMode::Rotate:
            operation = ImGuizmo::ROTATE;
            break;
        case EditorState::GizmoMode::Scale:
            operation = ImGuizmo::SCALE;
            break;
    }

    // Gizmo space
    ImGuizmo::MODE mode = (m_EditorState->GetGizmoSpace() == EditorState::GizmoSpace::World)
        ? ImGuizmo::WORLD
        : ImGuizmo::LOCAL;

    // Build transform matrix from entity transform
    // ImGuizmo expects a 4x4 matrix in column-major order
    Mat4 matrix = entityTransform.worldMatrix;

    // Manipulate transform
    bool isManipulating = ImGuizmo::Manipulate(
        &camera.viewMatrix[0][0],
        &camera.projectionMatrix[0][0],
        operation,
        mode,
        &matrix[0][0],
        nullptr,  // deltaMatrix
        nullptr   // snap
    );

    // Update entity transform if gizmo was manipulated
    if (isManipulating)
    {
        m_EditorState->SetManipulating(true);

        // Decompose matrix to get position, rotation, scale
        Vec3 position, rotation, scale;
        ImGuizmo::DecomposeMatrixToComponents(
            &matrix[0][0],
            &position.x,
            &rotation.x,
            &scale.x
        );

        // Apply to local transform
        // Note: If entity has a parent, we'd need to convert world to local space
        // For now, assume we're working in world space
        entityTransform.localPosition = position;
        entityTransform.localRotation = QuatFromEuler(Vec3(
            Radians(rotation.x),
            Radians(rotation.y),
            Radians(rotation.z)
        ));
        entityTransform.localScale = scale;
        entityTransform.isDirty = true;
    }
    else if (m_EditorState->IsManipulating())
    {
        // Just finished manipulating
        m_EditorState->SetManipulating(false);
    }
}

#endif // _DEBUG
