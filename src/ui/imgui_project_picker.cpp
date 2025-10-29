#include "imgui_project_picker.h"
#include "core/project_manager.h"
#include "core/file_dialog.h"

#include <GL/glew.h>  // Must be included before GLFW
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <filesystem>
#include <cstring>

namespace fs = std::filesystem;

ImGuiProjectPicker::ImGuiProjectPicker(ProjectManager* projectMgr)
    : m_ProjectManager(projectMgr)
{
}

ImGuiProjectPicker::~ImGuiProjectPicker() {
    Shutdown();
}

bool ImGuiProjectPicker::Init() {
    if (m_Initialized) {
        std::cerr << "ImGuiProjectPicker already initialized" << std::endl;
        return false;
    }

    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    // Setup OpenGL 3.3 context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);  // Non-resizable window

    // Create window with native decorations (title bar with minimize/close buttons)
    m_Window = glfwCreateWindow(800, 600, "Project Selection", NULL, NULL);
    if (!m_Window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_Window);
    glfwSwapInterval(1);  // Enable VSync

    // Initialize GLEW
    glewExperimental = GL_TRUE;
    GLenum glewError = glewInit();
    if (glewError != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(glewError) << std::endl;
        glfwDestroyWindow(m_Window);
        glfwTerminate();
        return false;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    m_Initialized = true;
    return true;
}

void ImGuiProjectPicker::Shutdown() {
    if (!m_Initialized) {
        return;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_Window) {
        glfwDestroyWindow(m_Window);
        m_Window = nullptr;
    }

    glfwTerminate();
    m_Initialized = false;
}

ProjectPickerResult ImGuiProjectPicker::Show() {
    if (!Init()) {
        ProjectPickerResult result;
        result.cancelled = true;
        return result;
    }

    ProjectPickerResult result;
    bool running = true;

    while (running) {
        // Poll events
        glfwPollEvents();

        // Check if window should close
        if (glfwWindowShouldClose(m_Window)) {
            result.cancelled = true;
            running = false;
            break;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Render main window
        RenderMainWindow();

        // Render create dialog if open
        if (m_ShowCreateDialog) {
            RenderCreateDialog();
        }

        // Render error popup if needed
        if (m_ShowError) {
            RenderErrorPopup();
        }

        // Check if user selected a project
        if (m_ProjectManager->HasActiveProject()) {
            result.success = true;
            result.projectPath = m_ProjectManager->GetProjectFilePath();
            result.setAsDefault = m_SetAsDefault;
            running = false;
        }

        // Rendering
        ImGui::Render();
        i32 displayW = 0;
        i32 displayH = 0;
        glfwGetFramebufferSize(m_Window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);  // Transparent background
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(m_Window);
    }

    return result;
}

void ImGuiProjectPicker::RenderMainWindow() {
    i32 windowWidth = 0;
    i32 windowHeight = 0;
    glfwGetWindowSize(m_Window, &windowWidth, &windowHeight);

    // Make ImGui window fill the entire GLFW window
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(static_cast<f32>(windowWidth), static_cast<f32>(windowHeight)), ImGuiCond_Always);

    // Remove title bar and borders since we're using the native window decorations
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar |
                                   ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("Project Selection", nullptr, windowFlags)) {
        // Title
        ImGui::Spacing();
        ImGui::SetCursorPosX((windowWidth - ImGui::CalcTextSize("Select or Create a Project").x) * 0.5f);
        ImGui::Text("Select or Create a Project");

        ImGui::Spacing();
        ImGui::Spacing();

        // Recent Projects Section
        ImGui::Text("Recent Projects:");
        ImGui::Spacing();

        RenderRecentProjects();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Action Buttons
        f32 buttonWidth = (windowWidth - 40.0f) * 0.5f;
        if (ImGui::Button("Create New Project", ImVec2(buttonWidth, 40))) {
            m_ShowCreateDialog = true;
            m_NewProjectName[0] = '\0';
            m_NewProjectPath[0] = '\0';
        }
        ImGui::SameLine();
        if (ImGui::Button("Open Existing Project", ImVec2(buttonWidth, 40))) {
            OnOpenProject();
        }

        ImGui::Spacing();

        // Set as Default Checkbox
        ImGui::Checkbox("Set as default project (skip this dialog next time)", &m_SetAsDefault);

        ImGui::Spacing();

        // Quit Button
        if (ImGui::Button("Quit", ImVec2(-1, 30))) {
            glfwSetWindowShouldClose(m_Window, GLFW_TRUE);
        }
    }
    ImGui::End();
}

void ImGuiProjectPicker::RenderRecentProjects() {
    const auto& recentProjects = m_ProjectManager->GetRecentProjects();

    if (recentProjects.empty()) {
        ImGui::TextDisabled("No recent projects");
    } else {
        // Create a scrolling region for recent projects
        ImGui::BeginChild("RecentProjectsList", ImVec2(0, 200), true);

        for (u32 i = 0; i < recentProjects.size(); ++i) {
            const std::string& projectPath = recentProjects[i];

            // Extract project name from path
            std::string projectName = "Unknown Project";
            try {
                fs::path p(projectPath);
                if (p.has_parent_path()) {
                    projectName = p.parent_path().filename().string();
                }
            } catch (...) {
                projectName = projectPath;
            }

            ImGui::PushID(static_cast<i32>(i));
            if (ImGui::Button(projectName.c_str(), ImVec2(-1, 0))) {
                OnSelectRecentProject(static_cast<i32>(i));
            }
            ImGui::PopID();

            // Show path as smaller text
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
            ImGui::TextWrapped("%s", projectPath.c_str());
            ImGui::PopStyleColor();

            ImGui::Spacing();
        }

        ImGui::EndChild();
    }
}

void ImGuiProjectPicker::RenderCreateDialog() {
    i32 windowWidth = 0;
    i32 windowHeight = 0;
    glfwGetWindowSize(m_Window, &windowWidth, &windowHeight);

    const f32 dialogW = 500.0f;
    const f32 dialogH = 250.0f;
    const f32 dialogX = (windowWidth - dialogW) * 0.5f;
    const f32 dialogY = (windowHeight - dialogH) * 0.5f;

    ImGui::SetNextWindowPos(ImVec2(dialogX, dialogY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(dialogW, dialogH), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("Create New Project", nullptr, flags)) {
        ImGui::Text("Project Name:");
        ImGui::InputText("##ProjectName", m_NewProjectName, sizeof(m_NewProjectName));

        ImGui::Spacing();

        ImGui::Text("Project Folder:");
        ImGui::InputText("##ProjectPath", m_NewProjectPath, sizeof(m_NewProjectPath));
        ImGui::SameLine();
        if (ImGui::Button("Browse...")) {
            auto folderPath = FileDialog::SelectFolder("Select Project Folder", "");
            if (folderPath.has_value()) {
                strncpy(m_NewProjectPath, folderPath.value().c_str(), sizeof(m_NewProjectPath) - 1);
                m_NewProjectPath[sizeof(m_NewProjectPath) - 1] = '\0';
            }
        }

        ImGui::Spacing();
        ImGui::Spacing();

        f32 buttonWidth = (dialogW - 40.0f) * 0.5f;
        if (ImGui::Button("Create", ImVec2(buttonWidth, 0))) {
            OnCreateProject();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) {
            m_ShowCreateDialog = false;
        }
    }
    ImGui::End();
}

void ImGuiProjectPicker::RenderErrorPopup() {
    i32 windowWidth = 0;
    i32 windowHeight = 0;
    glfwGetWindowSize(m_Window, &windowWidth, &windowHeight);

    const f32 popupW = 400.0f;
    const f32 popupH = 150.0f;
    const f32 popupX = (windowWidth - popupW) * 0.5f;
    const f32 popupY = (windowHeight - popupH) * 0.5f;

    ImGui::SetNextWindowPos(ImVec2(popupX, popupY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(popupW, popupH), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("Error", nullptr, flags)) {
        ImGui::TextWrapped("%s", m_ErrorMessage.c_str());

        ImGui::Spacing();
        ImGui::Spacing();

        if (ImGui::Button("OK", ImVec2(-1, 0))) {
            m_ShowError = false;
        }
    }
    ImGui::End();
}

void ImGuiProjectPicker::OnCreateProject() {
    // Validate inputs
    if (strlen(m_NewProjectName) == 0) {
        ShowError("Please enter a project name");
        return;
    }

    if (strlen(m_NewProjectPath) == 0) {
        ShowError("Please select a project folder");
        return;
    }

    // Create project
    std::string projectName(m_NewProjectName);
    std::string projectPath(m_NewProjectPath);

    if (!m_ProjectManager->CreateProject(projectPath, projectName)) {
        ShowError("Failed to create project. Make sure the folder is empty or doesn't exist.");
        return;
    }

    m_ShowCreateDialog = false;
}

void ImGuiProjectPicker::OnOpenProject() {
    auto projectFile = FileDialog::OpenFile("Open Project", "", {"Project Files", "*.json"});
    if (projectFile.has_value()) {
        if (!m_ProjectManager->LoadProject(projectFile.value())) {
            ShowError("Failed to load project file");
        }
    }
}

void ImGuiProjectPicker::OnSelectRecentProject(i32 index) {
    const auto& recentProjects = m_ProjectManager->GetRecentProjects();
    if (index >= 0 && index < static_cast<i32>(recentProjects.size())) {
        const std::string& projectPath = recentProjects[index];
        if (!m_ProjectManager->LoadProject(projectPath)) {
            ShowError("Failed to load project. The project file may have been moved or deleted.");
        }
    }
}

void ImGuiProjectPicker::ShowError(const std::string& message) {
    m_ErrorMessage = message;
    m_ShowError = true;
}
