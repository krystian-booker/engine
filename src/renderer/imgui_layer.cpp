#include "imgui_layer.h"

#ifdef _DEBUG

#include "vulkan_context.h"
#include "platform/window.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <stdexcept>

void ImGuiLayer::Init(VulkanContext* context, Window* window, VkRenderPass renderPass)
{
    m_Context = context;
    m_Window = window;

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
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Exit", "Alt+F4"))
            {
                // Signal to close the window
                glfwSetWindowShouldClose(m_Window->GetNativeWindow(), GLFW_TRUE);
            }
            ImGui::EndMenu();
        }

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

#endif // _DEBUG
